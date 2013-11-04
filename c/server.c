#include "../bad.h"
int main(void) {
    struct in_addr ip = { .s_addr = htonl(INADDR_ANY) };
    struct shared_pool *sp = shared_pool_init(0xbeef,17);
    t_bind_and_wait_for_updates(sp,ip,1234);
    shared_pool_destroy(sp);
    return 0;
}
