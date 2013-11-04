#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef BAD_DIE
#define BAD_DIE(fmt,arg...) \
do {                         \
    E(fmt,##arg);            \
    exit(EXIT_FAILURE);      \
} while(0)
#endif

#define FORMAT(fmt,arg...) fmt " [%s():%s:%d @ %u]\n",##arg,__func__,__FILE__,__LINE__,(unsigned int) time(NULL)
#define E(fmt,arg...) fprintf(stderr,FORMAT(fmt,##arg))
#define D(fmt,arg...) printf(FORMAT(fmt,##arg))
#define SAYX(fmt,arg...) do {         \
        BAD_DIE(fmt,##arg);          \
} while(0)

#define SAYPX(fmt,arg...) SAYX(fmt " { %s(%d) }",##arg,errno ? strerror(errno) : "undefined error",errno);

#define MAX_PACKET_SIZE 65000
#define POOL_SIZE(sp)  (sizeof(struct item) * ((sp)->hash_mask + 1))

#define FNV_PRIME_32 16777619
#define FNV_OFFSET_32 2166136261U

#define KEY_SIZE 32
#define BLOB_SIZE 128

#ifndef BAD_ALLOC
#define BAD_ALLOC(s,cast)               \
do {                                     \
    s = (cast *) x_malloc(sizeof(cast)); \
} while(0)

void *x_malloc(size_t s) {
    void *x = malloc(s);
    if (!x)
        SAYPX("malloc for %zu failed",s);
    return x;
}

#endif

#ifndef BAD_FREE
#define BAD_FREE(s) x_free(s)
void x_free(void *x) {
    if (x != NULL)
        free(x);
}
#endif

static struct sembuf sem_lock = { 0, -1, 0 };
static struct sembuf sem_unlock = { 0, 1, 0 };

struct item {
    uint8_t key[KEY_SIZE];
    uint32_t key_len;
    uint8_t blob[BLOB_SIZE];
    uint32_t blob_len;
} __attribute__((packed));

struct shared_pool {
    int shm_id;
    int sem_id;
    int key;
    int hash_mask;
    int hash_shift;
    struct item *pool;
    int c_sock;
    int s_sock;
};

static void shared_pool_destroy(struct shared_pool *);


inline uint32_t FNV32(struct shared_pool *sp, const char *s,size_t len) {
    uint32_t hash = FNV_OFFSET_32, i;
    for(i = 0; i < len; i++) {
        hash = hash ^ (s[i]);
        hash = hash * FNV_PRIME_32;
    }
    return (hash >> sp->hash_shift) ^ (hash & sp->hash_mask);
}                         

static inline void shared_pool_lock(struct shared_pool *sp) {
    if (semop(sp->sem_id,&sem_lock, 1) == -1)
        SAYPX("semop");
}

static inline void shared_pool_unlock(struct shared_pool *sp) {
    if (semop(sp->sem_id,&sem_unlock, 1) == -1)
        SAYPX("semop");
}

static inline int shared_pool_unlock_and_return_err(struct shared_pool *sp, int err) {
    shared_pool_unlock(sp);
    return err;
}

inline static struct item *t_find_locked(struct shared_pool *sp, char *key, size_t klen) {
    uint32_t index = FNV32(sp,key,klen);
    struct item *item = &sp->pool[index];
    if (klen > 0 && item->key_len == klen && memcmp(item->key,key,klen) == 0)
        return item;
    return NULL;
}

struct item *t_add_locked(struct shared_pool *sp,char *key, size_t klen, uint8_t *p, size_t len) {
    if (len <= 0 || len >= BLOB_SIZE || klen <= 0 || klen >= KEY_SIZE)
        return NULL;

    uint32_t index = FNV32(sp,key,klen);
    struct item *item = &sp->pool[index];
    item->key_len = klen;
    item->blob_len = len;
    memcpy(item->key,key,klen);
    memcpy(item->blob,p,len);
    return item;
}

inline static int t_add(struct shared_pool *sp,char *key, size_t klen, uint8_t *p, size_t len) {
    shared_pool_lock(sp);
    struct item *item = t_add_locked(sp,key,klen,p,len);
    return shared_pool_unlock_and_return_err(sp,item ? 0 : -EFAULT);
}

inline static int t_add_and_send_to(struct shared_pool *sp,char *key, size_t klen, uint8_t *p, size_t len,struct in_addr ip, uint16_t port) {  
    if (sp->c_sock == 0) {
        if ((sp->c_sock = socket(AF_INET,SOCK_DGRAM,0)) < 0)
            SAYPX("socket");
        int op = 1;
        setsockopt(sp->c_sock, SOL_SOCKET, SO_BROADCAST, (char *) &op, sizeof(op));
    }

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = ip;
    addr.sin_port = htons(port);
    shared_pool_lock(sp);
    struct item *item = t_add_locked(sp,key,klen,p,len);
    if (item == NULL)
        return shared_pool_unlock_and_return_err(sp,-EFAULT);

    int rc = sendto(sp->c_sock,item,sizeof(*item),0,(struct sockaddr *)&addr,sizeof(addr));
    if (rc != sizeof(*item))
        return shared_pool_unlock_and_return_err(sp,rc);

    return shared_pool_unlock_and_return_err(sp,0);
}

static void t_bind_and_wait_for_updates(struct shared_pool *sp, struct in_addr ip, uint16_t port) {
    if ((sp->s_sock = socket(AF_INET,SOCK_DGRAM,0)) < 0)
        SAYPX("socket");

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = ip;
    addr.sin_port = htons(port);

    if (bind(sp->s_sock,(struct sockaddr *)&addr,sizeof(addr)) < 0)
        SAYPX("bind failed on socket: %d on ip: %d",sp->s_sock,ip.s_addr);

    int rc;
    struct item value;
    for (;;) {
        rc = recv(sp->s_sock,&value,sizeof(value),MSG_WAITALL);
        if (rc == sizeof(value)) {
            if (t_add(sp,(char *)value.key,value.key_len,value.blob,value.blob_len) < 0)
                E("unable to store input key (klen: %d, blen: %d)",value.key_len,value.blob_len);
            else
                D("added remote key");
        } else {
            BAD_DIE("recv failed");
            break;
        }
    }
}

static struct shared_pool *shared_pool_init(int key, int hash_bits) {
    if (key % 2 == 0)
        SAYX("key must be odd number, key + 1 is used for the semaphore's key");

    if (hash_bits < 16 || hash_bits > 32 )
        SAYX("hash bits must be between 16 and 32");

    struct shared_pool *sp;
    BAD_ALLOC(sp,struct shared_pool);
    sp->c_sock = 0;
    sp->s_sock = 0;
    sp->pool = NULL;
    sp->hash_shift = hash_bits;
    sp->hash_mask = (((uint32_t) 1 << hash_bits) - 1);
    int flags = 0666 | IPC_CREAT;
    if ((sp->shm_id = shmget(key, sizeof(struct item) * (sp->hash_mask + 1), flags)) < 0)
        SAYPX("shmget: 0x%x",key);

    if ((sp->sem_id = semget(key + 1, 1, flags)) < 0)
        SAYPX("shmget: 0x%x",key + 1);

    if ((sp->pool = shmat(sp->shm_id, NULL, 0)) < 0 )
        SAYPX("shmat");

    sp->key = key;
    shared_pool_unlock(sp);
    return sp;
}

static void shared_pool_reset(struct shared_pool *sp) {
    shared_pool_lock(sp);
    int i;
    for (i = 0; i < sp->hash_mask + 1; i++) {
        sp->pool[i].key_len = 0;
        sp->pool[i].blob_len = 0;
    }
    shared_pool_unlock(sp);
}

static void shared_pool_destroy(struct shared_pool *sp) {
    shared_pool_lock(sp);
    if (shmdt(sp->pool) != 0)
        SAYPX("detach failed");
    
    struct shmid_ds ds;
    
    if (shmctl(sp->shm_id, IPC_STAT, &ds) != 0)
        SAYPX("IPC_STAT failed on sem_id %d",sp->sem_id);
    if (ds.shm_nattch == 0) {
        if (semctl(sp->sem_id, 0, IPC_RMID ) != 0) 
            SAYPX("IPC_RMID failed on sem_id %d",sp->sem_id);
        if (shmctl(sp->shm_id, IPC_RMID, NULL) != 0) 
            SAYPX("IPC_RMID failed on shm_id %d",sp->shm_id);
    } else {
       shared_pool_unlock(sp); 
    }

    BAD_FREE(sp);
}

