#ifndef __BAD_H__
#define __BAD_H__

#define XS_STATE(type, x) \
    INT2PTR(type, SvROK(x) ? SvIV(SvRV(x)) : SvIV(x))

#define XS_STRUCT2OBJ(sv, class, obj) \
    if (obj == NULL) { \
        sv_setsv(sv, &PL_sv_undef); \
    } else { \
        sv_setref_pv(sv, class, (void *) obj); \
    }
#define BAD_ALLOC(s,cast) Newx(s,1,cast)
#define BAD_FREE(s) Safefree(s)
#define BAD_DIE(fmt,arg...) die(fmt,##arg)

#include <bad.h>
typedef struct shared_pool BADContext;

#endif
