#include "ruby.h"
#define BAD_DIE(fmt,arg...) rb_raise(rb_eArgError,fmt,##arg) 

#include "xbad.h"
 

static VALUE XBAD_init(VALUE class, VALUE _shmid, VALUE _hash_bits, VALUE _item_size ) {
    int shmid = NUM2INT(_shmid);
    int item_size = NUM2INT(_item_size);
    int hash_bits = NUM2INT(_hash_bits);
    struct shared_pool *p = shared_pool_init(shmid,hash_bits,item_size);
    VALUE sp = Data_Wrap_Struct(class, 0, shared_pool_destroy, p);
    return sp;
}

inline struct shared_pool *shared_pool(VALUE obj) {
    struct shared_pool *sp;
    Data_Get_Struct(obj,struct shared_pool, sp);
    return sp;
}

static inline void die_unless_string(VALUE x) {
    if (TYPE(x) != T_STRING)
        rb_raise(rb_eTypeError, "invalid type for input %s",rb_obj_classname(x));
}

static VALUE XBAD_store(VALUE self, VALUE key, VALUE value, VALUE _expire_after) {
    die_unless_string(key);
    die_unless_string(value);
    struct shared_pool *sp = shared_pool(self);
    int expire_after = NUM2INT(_expire_after);
    if (t_add(sp,RSTRING_PTR(key),RSTRING_LEN(key),RSTRING_PTR(value),RSTRING_LEN(value),expire_after) != 0)
        rb_raise(rb_eArgError,"failed to store");
    return value;
}

static VALUE XBAD_store_and_broadcast(VALUE self, VALUE key, VALUE value, VALUE _expire_after,VALUE _port) {
    die_unless_string(key);
    die_unless_string(value);
    struct shared_pool *sp = shared_pool(self);
    int expire_after = NUM2INT(_expire_after);
    unsigned short port = NUM2INT(_port);
    struct in_addr ip = { .s_addr = htonl(0xffffffff) };                                                                                                                                                        

    if (t_add_and_sent_to(sp,RSTRING_PTR(key),RSTRING_LEN(key),RSTRING_PTR(value),RSTRING_LEN(value),expire_after,ip,port) != 0)
        rb_raise(rb_eArgError,"failed to store and broadcast");
    return value;
}

static VALUE XBAD_find(VALUE self, VALUE key) {
    VALUE ret = Qnil;
    struct shared_pool *sp = shared_pool(self);
    struct item *item = t_find_and_lock(sp,RSTRING_PTR(key),RSTRING_LEN(key));
    if (item) {
        ret = rb_str_new(ITEM_BLOB(item),item->blob_len);
        shared_pool_unlock_item(item);
    }
    return ret;
}

static VALUE XBAD_reset(VALUE self) {
    struct shared_pool *sp = shared_pool(self);
    shared_pool_reset(sp);
    return Qtrue;
}

void Init_xbad() {
  VALUE c = rb_define_class("XBAD", rb_cObject);
  rb_define_singleton_method(c, "create", RUBY_METHOD_FUNC(XBAD_init), 3);
  rb_define_method(c, "find", XBAD_find, 1);
  rb_define_method(c, "store", XBAD_store, 3);
  rb_define_method(c, "store_and_broadcast", XBAD_store_and_broadcast, 4);
}
