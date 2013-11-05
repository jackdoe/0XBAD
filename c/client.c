#include "../bad.h"
int main(void) {
    struct shared_pool *sp = shared_pool_init(0xbeef,17,128);
    struct in_addr ip = { .s_addr = 0xffffffff };
    if (t_add_and_send_to(sp,"aaa",3,(uint8_t *) "bbb",4,0,ip,1234) != 0)
        SAYX("failed to store");
    shared_pool_lock(sp);
    struct item *i = t_find_locked(sp,"aaa",3);
    if (i) {
        D("found item with klen: %d",i->key_len);
    }
    shared_pool_unlock(sp);

    shared_pool_reset(sp);
    shared_pool_lock(sp);
    i = t_find_locked(sp,"aaa",3);
    if (i) {
        D("we should not find this key");
    }
    shared_pool_unlock(sp);
    shared_pool_destroy(sp);
    return 0;
}
