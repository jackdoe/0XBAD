#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#ifndef BAD_DIE
#define BAD_DIE(fmt,arg...)  \
do {                         \
    E(fmt,##arg);            \
    exit(EXIT_FAILURE);      \
} while(0)
#endif

#define FORMAT(fmt,arg...) fmt " [%s():%s:%d @ %u]\n",##arg,__func__,__FILE__,__LINE__,(unsigned int) time(NULL)
#define E(fmt,arg...) fprintf(stderr,FORMAT(fmt,##arg))
#define D(fmt,arg...) printf(FORMAT(fmt,##arg))
#define SAYX(fmt,arg...) do {         \
        BAD_DIE(fmt,##arg);           \
} while(0)

#define SAYPX(fmt,arg...) SAYX(fmt " { %s(%d) }",##arg,errno ? strerror(errno) : "undefined error",errno);

#define ITEM(sp,index) (struct item *) (&(sp)->pool[(index * (sp)->item_size)])
#define ITEM_KEY(item) &(item)->blob[0]
#define ITEM_BLOB(item) &(item)->blob[(item)->key_len]

#define FNV_PRIME_32 16777619
#define FNV_OFFSET_32 2166136261U

#ifndef BAD_ALLOC
#define BAD_ALLOC(s,cast)                \
do {                                     \
    s = (cast *) x_malloc(sizeof(cast)); \
} while(0)

static void *x_malloc(size_t s) {
    void *x = malloc(s);
    if (!x)
        SAYPX("malloc for %zu failed",s);
    return x;
}

#endif

#ifndef BAD_FREE
#define BAD_FREE(s) x_free(s)

static void x_free(void *x) {
    if (x != NULL)
        free(x);
}
#endif

struct item {
    volatile int lock;
    uint32_t key_len;
    uint32_t blob_len;
    uint32_t expire_at;
    uint8_t blob[0];
} __attribute__((packed));

struct shared_pool {
    uint8_t *pool;
    uint32_t item_size;
    int shm_id;
    int key;
    int hash_mask;
    int hash_shift;
    int c_sock;
    int s_sock;
};

inline uint32_t FNV32(struct shared_pool *sp, const char *s,size_t len) {
    uint32_t hash = FNV_OFFSET_32, i;
    for(i = 0; i < len; i++) {
        hash = hash ^ (s[i]);
        hash = hash * FNV_PRIME_32;
    }
    return (hash >> sp->hash_shift) ^ (hash & sp->hash_mask);
}                         

static inline void shared_pool_lock_item(struct item *item) {
    while (__sync_lock_test_and_set(&item->lock, 1)) {
        while(item->lock)
            ; // spin with less stress
    }
}

static inline void shared_pool_unlock_item(struct item *item) {
    __sync_lock_release(&item->lock);
}

static inline struct item *t_find_and_lock(struct shared_pool *sp, char *key, size_t klen) {
    uint32_t index = FNV32(sp,key,klen);
    struct item *item = ITEM(sp,index);
    shared_pool_lock_item(item);
    if ((item->expire_at == 0 || item->expire_at > time(NULL)) && klen > 0 && item->key_len == klen && memcmp(ITEM_KEY(item),key,klen) == 0)
        return item;
    shared_pool_unlock_item(item);
    return NULL;
}

static inline struct item *t_add_and_lock(struct shared_pool *sp,char *key, size_t klen, uint8_t *p, size_t len,uint32_t expire_after) {
    if (len <= 0 || len + sizeof(struct item) >= sp->item_size)
        return NULL;
    uint32_t index = FNV32(sp,key,klen);
    struct item *item = ITEM(sp,index);
    shared_pool_lock_item(item);
    item->key_len = klen;
    item->blob_len = len;
    if (expire_after > 0)
        item->expire_at = time(NULL) + expire_after;
    else
        item->expire_at = 0;

    memcpy(ITEM_KEY(item),key,klen);
    memcpy(ITEM_BLOB(item),p,len);
    return item;
}

static inline int t_add(struct shared_pool *sp,char *key, size_t klen, uint8_t *p, size_t len,uint32_t expire_after) {
    struct item *item = t_add_and_lock(sp,key,klen,p,len,expire_after);
    if (item != NULL) {
        shared_pool_unlock_item(item);
        return 0;
    } else {
        return -EFAULT;
    }
}
static inline int t_add_and_send_to(struct shared_pool *sp,char *key, size_t klen, uint8_t *p, size_t len,uint32_t expire_after,struct in_addr ip, uint16_t port) {  
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
    struct item *item = t_add_and_lock(sp,key,klen,p,len,expire_after);
    if (item == NULL)
        return -EFAULT;

    int rc = sendto(sp->c_sock,item,sp->item_size,0,(struct sockaddr *)&addr,sizeof(addr));
    shared_pool_unlock_item(item);
    if (rc != sp->item_size) 
        return -EFAULT;
    return 0;
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
    uint8_t buf[sp->item_size];
    for (;;) {
        rc = recv(sp->s_sock,buf,sp->item_size,MSG_WAITALL);
        if (rc == sp->item_size) {
            struct item *value = (struct item *) buf;
            uint32_t expire_after = value->expire_at > 0 ? value->expire_at - time(NULL) : 0; 
            if (t_add(sp,(char *)ITEM_KEY(value),value->key_len,ITEM_BLOB(value),value->blob_len,expire_after) < 0)
                E("unable to store input key (klen: %d, blen: %d)",value->key_len,value->blob_len);
            else
                D("added remote key");
        } else {
            if (rc > 0) {
                E("received bad frame expected item size: %d got %d",sp->item_size,rc);
            } else {
                BAD_DIE("recv failed");
                break;
            }
        }
    }
}

static struct shared_pool *shared_pool_alloc(void) {
    struct shared_pool *sp;
    BAD_ALLOC(sp,struct shared_pool);
    memset(sp,0,sizeof(*sp));
    return sp;
}

static struct shared_pool *shared_pool_init(struct shared_pool *sp, int key, int hash_bits, int item_size) {
    if (hash_bits < 16 || hash_bits > 32 )
        SAYX("hash bits must be between 16 and 32");
 
    if (item_size <= sizeof(struct item))
        SAYX("item size must be at least %zu",sizeof(struct item));

    sp->c_sock = 0;
    sp->s_sock = 0;
    sp->pool = NULL;
    sp->item_size = item_size;
    sp->hash_shift = hash_bits;
    sp->hash_mask = (((uint32_t) 1 << hash_bits) - 1);
    sp->key = key;

    int flags = 0666 | IPC_CREAT;
    #define SHMGET(sp) shmget(sp->key, sp->item_size * (sp->hash_mask + 1), flags)
    if ((sp->shm_id = SHMGET(sp)) < 0)
        SAYPX("shmget: 0x%x",key);
    #undef SHMGET

    if ((sp->pool = shmat(sp->shm_id, NULL, 0)) < 0 )
        SAYPX("shmat");

    return sp;
}

static struct shared_pool *shared_pool_alloc_and_init(int key, int hash_bits, int item_size) {
    struct shared_pool *sp = shared_pool_alloc();
    return shared_pool_init(sp,key,hash_bits,item_size);
}
static void shared_pool_reset(struct shared_pool *sp) {
    int i;
    for (i = 0; i < sp->hash_mask + 1; i++) {
        struct item *item = ITEM(sp,i);
        item->expire_at = 1;
    }
}

// this is really bad - please dont use it unless you want to debug something
static void shared_pool_blind_guardian(struct shared_pool *sp, int interval_us,int cycles,int warn_only) {
    int i,attempts;
    struct item *item;
    for (;;) {
        for (i = 0; i < sp->hash_mask + 1; i++) {
            item = ITEM(sp,i);
            if (item->key_len > 0) {
                attempts = 0;
                while (item->lock == 1) { // no barrier
                    if (attempts++ > cycles) {
                        char buf[BUFSIZ];
                        int len = item->key_len > BUFSIZ - 1 ? BUFSIZ - 1 : item->key_len;
                        memcpy(buf,ITEM_KEY(item),len);
                        buf[len] = '\0';
                        E("unlocking - %s, locked for more then %d cycles",buf,cycles);
                        if (!warn_only) {
                            item->lock = 0;
                            item->expire_at = 1;
                        }
                    }
                }
            }
        }
        usleep(interval_us);
    }
}

static void shared_pool_destroy(struct shared_pool *sp) {
    if (sp->pool) {
        if (shmdt(sp->pool) != 0)
            SAYPX("detach failed");
        
        struct shmid_ds ds;

        if (shmctl(sp->shm_id, IPC_STAT, &ds) != 0)
            SAYPX("IPC_STAT failed");
        if (ds.shm_nattch == 0) {
            if (shmctl(sp->shm_id, IPC_RMID, NULL) != 0) 
                SAYPX("IPC_RMID failed on shm_id %d",sp->shm_id);
        }
    }
    BAD_FREE(sp);
}

