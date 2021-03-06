#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#define DIE_UNLESS_STRING(x)            \
do {                                    \
    if (!SvPOKp((x)))                   \
       die("string is required");       \
} while(0)

#include "BAD.h"

MODULE = BAD PACKAGE = BAD		

PROTOTYPES: DISABLED

BADContext*
BAD::new(key,hash_bits,item_size)
    int key
    int hash_bits
    int item_size
    PREINIT:
        BADContext* ctx;
    CODE:
        ctx = shared_pool_alloc_and_init(key,hash_bits,item_size);
        RETVAL=ctx;
    OUTPUT:
        RETVAL

void
DESTROY(BADContext* ctx)
    CODE:
        shared_pool_destroy(ctx);

SV*
find(BADContext* ctx, SV *key)
    CODE:
        STRLEN len;
        char *p;
        p = SvPV(key, len);
        struct item *i = t_find_and_lock(ctx,p,len);
        if (i) {
            RETVAL = newSVpv(ITEM_BLOB(i),i->blob_len); 
            shared_pool_unlock_item(i);
        } else {
            RETVAL = &PL_sv_undef;
        }
    OUTPUT:
        RETVAL

void
store(BADContext *ctx, SV *_key, SV* _value, int expire_after)
    PPCODE:
        DIE_UNLESS_STRING(_key);
        DIE_UNLESS_STRING(_value);

        STRLEN len,klen;
        char *key,*value;
        value = SvPV(_value, len);
        key = SvPV(_key, klen);

        if (t_add(ctx,key,klen,value,len,expire_after) != 0)
            die("unable to store item");
        XPUSHs(_value);

void
store_and_broadcast(BADContext *ctx, SV *_key, SV* _value, int expire_after, unsigned short port)
    PPCODE:
        DIE_UNLESS_STRING(_key);
        DIE_UNLESS_STRING(_value);

        STRLEN len,klen;
        char *key,*value;
        value = SvPV(_value, len);
        key = SvPV(_key, klen);
        struct in_addr ip = { .s_addr = htonl(0xffffffff) };
        int rc = t_add_and_send_to(ctx,key,klen,value,len,expire_after,ip,port);
        if (rc < 0)
            die("unable to store item");
        XPUSHs(_value);

void bind_and_wait_for_updates(BADContext *ctx,unsigned short port)
    CODE:
        struct in_addr ip = { .s_addr = htonl(INADDR_ANY) };
        t_bind_and_wait_for_updates(ctx,ip,port);

void
reset(BADContext *ctx)
    CODE:
        shared_pool_reset(ctx);


