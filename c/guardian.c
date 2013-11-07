#include "../bad.h"


int main(void) {
    struct shared_pool *sp = shared_pool_init(0xbeef,17,128);
    shared_pool_blind_guardian(sp,10000,10000000,0);
    shared_pool_destroy(sp);
    return 0;
}
