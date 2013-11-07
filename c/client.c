#include "../bad.h"
#include <time.h>


int main(void) {
    struct shared_pool *sp = shared_pool_init(0xbeef,17,128);
    struct in_addr ip = { .s_addr = 0xffffffff };
    if (t_add_and_send_to(sp,"aaa",3,(uint8_t *) "bbb",4,0,ip,1234) < 0)
        SAYX("failed to store");
    struct timespec tstart={0,0}, tend={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    int j;
    int every = 10000000;
    for (j = 0;;j++) {
        if (j % every == 0) {

            clock_gettime(CLOCK_MONOTONIC, &tend);
            double ms = ((double)tend.tv_sec + (1.0e-9*tend.tv_nsec)) - ((double)tstart.tv_sec + (1.0e-9*tstart.tv_nsec));
            D("took %.5f seconds for %d, %.5f per second",ms,every,every/ms);
            tstart = tend;
        }
        struct item *i = t_find_and_lock(sp,"aaa",3);
        if (i) {
        //    sleep(100);
            shared_pool_unlock_item(i);
        }
    }

/*
 *
            D("found item with klen: %d",i->key_len);
    shared_pool_reset(sp);
    shared_pool_lock(sp);
    i = t_find_locked(sp,"aaa",3);
    if (i) {
        D("we should not find this key");
    }
    shared_pool_unlock(sp);
    */
    shared_pool_destroy(sp);
    return 0;
}
