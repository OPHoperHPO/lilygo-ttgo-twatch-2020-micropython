// This is bindings for micropython for RMTLib [https://github.com/haxplore/ESP32_RMT_IRLib]
// License: MIT
#include <stdlib.h>
#include <string.h>
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/objarray.h"

/*
 * ir includes
 */

#include "RMTLinks.h"


/*
 * Helper functions
 */

// Custom function mp object

typedef struct _mp_lv_obj_fun_builtin_var_t {
    mp_obj_base_t base;
    mp_uint_t n_args;
    mp_fun_var_t mp_fun;
    void *lv_fun;
} mp_lv_obj_fun_builtin_var_t;

STATIC mp_obj_t lv_fun_builtin_var_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args);
STATIC mp_int_t mp_func_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags);

STATIC const mp_obj_type_t mp_lv_type_fun_builtin_var = {
    { &mp_type_type },
    .flags = MP_TYPE_FLAG_BINDS_SELF | MP_TYPE_FLAG_BUILTIN_FUN,
    .name = MP_QSTR_function,
    .call = lv_fun_builtin_var_call,
    .unary_op = mp_generic_unary_op,
    .buffer_p = { .get_buffer = mp_func_get_buffer }
};

STATIC mp_obj_t lv_fun_builtin_var_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    assert(MP_OBJ_IS_TYPE(self_in, &mp_lv_type_fun_builtin_var));
    mp_lv_obj_fun_builtin_var_t *self = MP_OBJ_TO_PTR(self_in);
    mp_arg_check_num(n_args, n_kw, self->n_args, self->n_args, false);
    return self->mp_fun(n_args, args);
}

STATIC mp_int_t mp_func_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    assert(MP_OBJ_IS_TYPE(self_in, &mp_lv_type_fun_builtin_var));
    mp_lv_obj_fun_builtin_var_t *self = MP_OBJ_TO_PTR(self_in);

    bufinfo->buf = &self->lv_fun;
    bufinfo->len = sizeof(self->lv_fun);
    bufinfo->typecode = BYTEARRAY_TYPECODE;
    return 0;
}

#define MP_DEFINE_CONST_LV_FUN_OBJ_VAR(obj_name, n_args, mp_fun, lv_fun) \
    const mp_lv_obj_fun_builtin_var_t obj_name = \
        {{&mp_lv_type_fun_builtin_var}, n_args, mp_fun, lv_fun}

// Casting

typedef struct mp_lv_struct_t
{
    mp_obj_base_t base;
    void *data;
} mp_lv_struct_t;

STATIC const mp_lv_struct_t mp_lv_null_obj;

#ifdef LV_OBJ_T
STATIC mp_int_t mp_lv_obj_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags);
#else
STATIC mp_int_t mp_lv_obj_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags){ return 0; }
#endif

STATIC mp_obj_t get_native_obj(mp_obj_t *mp_obj)
{
    if (!MP_OBJ_IS_OBJ(mp_obj)) return mp_obj;
    const mp_obj_type_t *native_type = ((mp_obj_base_t*)mp_obj)->type;
    if (native_type->parent == NULL || 
        (native_type->buffer_p.get_buffer == mp_lv_obj_get_buffer)) return mp_obj;
    while (native_type->parent) native_type = native_type->parent;
    return mp_obj_cast_to_native_base(mp_obj, MP_OBJ_FROM_PTR(native_type));
}

STATIC mp_obj_t dict_to_struct(mp_obj_t dict, const mp_obj_type_t *type);

STATIC mp_obj_t make_new_lv_struct(
    const mp_obj_type_t *type,
    size_t n_args,
    size_t n_kw,
    const mp_obj_t *args);

STATIC mp_obj_t *cast(mp_obj_t *mp_obj, const mp_obj_type_t *mp_type)
{
    mp_obj_t *res = NULL;
    if (mp_obj == mp_const_none && mp_type->make_new == &make_new_lv_struct) {
        res = MP_OBJ_FROM_PTR(&mp_lv_null_obj);
    } else if (MP_OBJ_IS_OBJ(mp_obj)) {
        res = get_native_obj(mp_obj);
        if (res){
            const mp_obj_type_t *res_type = ((mp_obj_base_t*)res)->type;
            if (res_type != mp_type){
                if (res_type == &mp_type_dict &&
                    mp_type->make_new == &make_new_lv_struct)
                        res = dict_to_struct(res, mp_type);
                else res = NULL;
            }
        }
    }
    if (res == NULL) nlr_raise(
        mp_obj_new_exception_msg_varg(
            &mp_type_SyntaxError, MP_ERROR_TEXT("Can't convert %s to %s!"), mp_obj_get_type_str(mp_obj), qstr_str(mp_type->name)));
    return res;
}

// object handling
// This section is enabled only when objects are supported 

#ifdef LV_OBJ_T

typedef LV_OBJ_T* (*lv_create)(LV_OBJ_T * par, const LV_OBJ_T * copy);

typedef struct mp_lv_obj_t {
    mp_obj_base_t base;
    LV_OBJ_T *lv_obj;
    LV_OBJ_T *callbacks;
} mp_lv_obj_t;

STATIC inline LV_OBJ_T *mp_to_lv(mp_obj_t *mp_obj)
{
    if (mp_obj == NULL || mp_obj == mp_const_none) return NULL;
    mp_lv_obj_t *mp_lv_obj = MP_OBJ_TO_PTR(get_native_obj(mp_obj));
    return mp_lv_obj->lv_obj;
}

STATIC inline LV_OBJ_T *mp_get_callbacks(mp_obj_t mp_obj)
{
    if (mp_obj == NULL || mp_obj == mp_const_none) return NULL;
    mp_lv_obj_t *mp_lv_obj = MP_OBJ_TO_PTR(get_native_obj(mp_obj));
    if (!mp_lv_obj->callbacks) mp_lv_obj->callbacks = mp_obj_new_dict(0);
    return mp_lv_obj->callbacks;
}

STATIC inline const mp_obj_type_t *get_BaseObj_type();

STATIC inline mp_obj_t *lv_to_mp(LV_OBJ_T *lv_obj)
{
    if (lv_obj == NULL) return mp_const_none;
    mp_lv_obj_t *self = (mp_lv_obj_t*)lv_obj->user_data;
    if (!self) 
    {
        self = m_new_obj(mp_lv_obj_t);
        *self = (mp_lv_obj_t){
            .base = {get_BaseObj_type()},
            .lv_obj = lv_obj,
            .callbacks = NULL,
        };
        lv_obj->user_data = self;
    }
    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t make_new(
    lv_create create,
    const mp_obj_type_t *type,
    size_t n_args,
    size_t n_kw,
    const mp_obj_t *args)
{
    mp_arg_check_num(n_args, n_kw, 0, 2, false);
    mp_lv_obj_t *self = m_new_obj(mp_lv_obj_t);
    LV_OBJ_T *parent = n_args > 0? mp_to_lv(args[0]): NULL;
    LV_OBJ_T *copy = n_args > 1? mp_to_lv(args[1]): NULL;
    *self = (mp_lv_obj_t){
        .base = {type}, 
        .lv_obj = create(parent, copy),
        .callbacks = NULL,
    };
    if (!self->lv_obj) return mp_const_none;
    self->lv_obj->user_data = self;
    return MP_OBJ_FROM_PTR(self);
}

STATIC void* mp_to_ptr(mp_obj_t self_in);

STATIC mp_obj_t cast_obj(mp_obj_t type_obj, mp_obj_t obj)
{
    mp_lv_obj_t *self = m_new_obj(mp_lv_obj_t);
    *self = (mp_lv_obj_t){
        .base = {(const mp_obj_type_t*)type_obj},
        .lv_obj = mp_to_ptr(obj),
        .callbacks = NULL,
    };
    if (!self->lv_obj) return mp_const_none;
    return MP_OBJ_FROM_PTR(self);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_2(cast_obj_obj, cast_obj);
STATIC MP_DEFINE_CONST_CLASSMETHOD_OBJ(cast_obj_class_method, MP_ROM_PTR(&cast_obj_obj));

STATIC mp_int_t mp_lv_obj_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    mp_lv_obj_t *self = MP_OBJ_TO_PTR(self_in);

    bufinfo->buf = &self->lv_obj;
    bufinfo->len = sizeof(self->lv_obj);
    bufinfo->typecode = BYTEARRAY_TYPECODE;
    return 0;
}

#endif

STATIC inline mp_obj_t convert_to_bool(bool b)
{
    return b? mp_const_true: mp_const_false;
}

STATIC inline mp_obj_t convert_to_str(const char *str)
{
    return str? mp_obj_new_str(str, strlen(str)): mp_const_none;
}

// struct handling

STATIC inline mp_lv_struct_t *mp_to_lv_struct(mp_obj_t mp_obj)
{
    if (mp_obj == NULL || mp_obj == mp_const_none) return NULL;
    if (!MP_OBJ_IS_OBJ(mp_obj)) nlr_raise(
            mp_obj_new_exception_msg(
                &mp_type_SyntaxError, MP_ERROR_TEXT("Struct argument is not an object!")));
    mp_lv_struct_t *mp_lv_struct = MP_OBJ_TO_PTR(get_native_obj(mp_obj));
    return mp_lv_struct;
}

STATIC inline size_t get_lv_struct_size(const mp_obj_type_t *type)
{
    mp_obj_t size_obj = mp_obj_dict_get(type->locals_dict, MP_OBJ_NEW_QSTR(MP_QSTR_SIZE));
    return (size_t)mp_obj_get_int(size_obj);
}

STATIC mp_obj_t make_new_lv_struct(
    const mp_obj_type_t *type,
    size_t n_args,
    size_t n_kw,
    const mp_obj_t *args)
{
    if ((!MP_OBJ_IS_TYPE(type, &mp_type_type)) || type->make_new != &make_new_lv_struct)
        nlr_raise(
            mp_obj_new_exception_msg(
                &mp_type_SyntaxError, MP_ERROR_TEXT("Argument is not a struct type!")));
    size_t size = get_lv_struct_size(type);
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    mp_lv_struct_t *self = m_new_obj(mp_lv_struct_t);
    *self = (mp_lv_struct_t){
        .base = {type}, 
        .data = m_malloc(size)
    };
    mp_lv_struct_t *other = n_args > 0? mp_to_lv_struct(cast(args[0], type)): NULL;
    if (other) {
        memcpy(self->data, other->data, size);
    } else {
        memset(self->data, 0, size);
    }
    return MP_OBJ_FROM_PTR(self);
}

STATIC void *copy_buffer(const void *buffer, size_t size)
{
    void *new_buffer = m_malloc(size);
    memcpy(new_buffer, buffer, size);
    return new_buffer;
}

// Reference an existing lv struct (or part of it)

STATIC mp_obj_t lv_to_mp_struct(const mp_obj_type_t *type, void *lv_struct)
{
    if (lv_struct == NULL) return mp_const_none;
    mp_lv_struct_t *self = m_new_obj(mp_lv_struct_t);
    *self = (mp_lv_struct_t){
        .base = {type},
        .data = lv_struct
    };
    return MP_OBJ_FROM_PTR(self);
}

STATIC void call_parent_methods(mp_obj_t obj, qstr attr, mp_obj_t *dest)
{
    const mp_obj_type_t *type = mp_obj_get_type(obj);
    while (type->locals_dict != NULL) {
        // generic method lookup
        // this is a lookup in the object (ie not class or type)
        assert(type->locals_dict->base.type == &mp_type_dict); // MicroPython restriction, for now
        mp_map_t *locals_map = &type->locals_dict->map;
        mp_map_elem_t *elem = mp_map_lookup(locals_map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
        if (elem != NULL) {
            mp_convert_member_lookup(obj, type, elem->value, dest);
            break;
        }
        if (type->parent == NULL) {
            break;
        }
        // search parents
        type = type->parent;
    }
}

// Convert dict to struct

STATIC mp_obj_t dict_to_struct(mp_obj_t dict, const mp_obj_type_t *type)
{
    mp_obj_t mp_struct = make_new_lv_struct(type, 0, 0, NULL);
    mp_obj_t *native_dict = cast(dict, &mp_type_dict);
    mp_map_t *map = mp_obj_dict_get_map(native_dict);
    if (map == NULL) return mp_const_none;
    for (uint i = 0; i < map->alloc; i++) {
        mp_obj_t key = map->table[i].key;
        mp_obj_t value = map->table[i].value;
        if (key != MP_OBJ_NULL) {
            mp_obj_t dest[] = {MP_OBJ_SENTINEL, value};
            type->attr(mp_struct, mp_obj_str_get_qstr(key), dest);
            if (dest[0]) nlr_raise(
                mp_obj_new_exception_msg_varg(
                    &mp_type_SyntaxError, MP_ERROR_TEXT("Cannot set field %s on struct %s!"), qstr_str(mp_obj_str_get_qstr(key)), qstr_str(type->name)));
        }
    }
    return mp_struct;
}

// Convert mp object to ptr

STATIC void* mp_to_ptr(mp_obj_t self_in)
{
    mp_buffer_info_t buffer_info;
    if (self_in == mp_const_none)
        return NULL;

//    if (MP_OBJ_IS_INT(self_in))
//        return (void*)mp_obj_get_int(self_in);

    if (!mp_get_buffer(self_in, &buffer_info, MP_BUFFER_READ)) {
        // No buffer protocol - this is not a Struct or a Blob, it's some other mp object.
        // We only allow setting dict directly, since it's useful to setting user_data for passing data to C.
        // On other cases throw an exception, to avoid a crash later
        if (MP_OBJ_IS_TYPE(self_in, &mp_type_dict))
            return MP_OBJ_TO_PTR(self_in);
        else nlr_raise(
                mp_obj_new_exception_msg_varg(
                    &mp_type_SyntaxError, MP_ERROR_TEXT("Cannot convert '%s' to pointer!"), mp_obj_get_type_str(self_in)));
    }

    if (MP_OBJ_IS_STR_OR_BYTES(self_in) || 
        MP_OBJ_IS_TYPE(self_in, &mp_type_bytearray) ||
        MP_OBJ_IS_TYPE(self_in, &mp_type_memoryview))
            return buffer_info.buf;
    else
    {
        void *result;
        if (buffer_info.len != sizeof(result) || buffer_info.typecode != BYTEARRAY_TYPECODE){
            nlr_raise(
                mp_obj_new_exception_msg_varg(
                    &mp_type_SyntaxError, MP_ERROR_TEXT("Cannot convert %s to pointer! (buffer does not represent a pointer)"), mp_obj_get_type_str(self_in)));
        }
        memcpy(&result, buffer_info.buf, sizeof(result));
        return result;
    }
}

// Blob is a wrapper for void* 

STATIC void mp_blob_print(const mp_print_t *print,
    mp_obj_t self_in,
    mp_print_kind_t kind)
{
    mp_printf(print, "Blob");
}

STATIC mp_int_t mp_blob_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    mp_lv_struct_t *self = MP_OBJ_TO_PTR(self_in);

    bufinfo->buf = &self->data;
    bufinfo->len = sizeof(self->data);
    bufinfo->typecode = BYTEARRAY_TYPECODE;
    return 0;
}

STATIC const mp_obj_fun_builtin_var_t mp_lv_dereference_obj;

// Sometimes (but not always!) Blob represents a Micropython object.
// In such cases it's safe to cast the Blob back to the Micropython object
// cast argument is the underlying object type, and it's optional.

STATIC mp_obj_t mp_blob_cast(size_t argc, const mp_obj_t *argv)
{
    mp_obj_t self = argv[0];
    void *ptr = mp_to_ptr(self);
    if (argc == 1) return MP_OBJ_FROM_PTR(ptr);
    mp_obj_t type = argv[1];
    if (!MP_OBJ_IS_TYPE(type, &mp_type_type))
        nlr_raise(
            mp_obj_new_exception_msg(
                &mp_type_SyntaxError, MP_ERROR_TEXT("Cast argument must be a type!")));
    return cast(MP_OBJ_FROM_PTR(ptr), type);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_blob_cast_obj, 1, 2, mp_blob_cast);

STATIC const mp_rom_map_elem_t mp_blob_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___dereference__), MP_ROM_PTR(&mp_lv_dereference_obj) },
    { MP_ROM_QSTR(MP_QSTR_cast), MP_ROM_PTR(&mp_blob_cast_obj) },
};

STATIC MP_DEFINE_CONST_DICT(mp_blob_locals_dict, mp_blob_locals_dict_table);

STATIC const mp_obj_type_t mp_blob_type = {
    { &mp_type_type },
    .name = MP_QSTR_Blob,
    .print = mp_blob_print,
    //.make_new = make_new_blob,
    .locals_dict = (mp_obj_dict_t*)&mp_blob_locals_dict,
    .buffer_p = { .get_buffer = mp_blob_get_buffer }
};

STATIC const mp_lv_struct_t mp_lv_null_obj = { {&mp_blob_type}, NULL };

STATIC inline mp_obj_t ptr_to_mp(void *data)
{
    return lv_to_mp_struct(&mp_blob_type, data);
}

// Cast pointer to struct

STATIC mp_obj_t mp_lv_cast(mp_obj_t type_obj, mp_obj_t ptr_obj)
{
    mp_lv_struct_t *self = m_new_obj(mp_lv_struct_t);
    *self = (mp_lv_struct_t){
        .base = {(const mp_obj_type_t*)type_obj}, 
        .data = mp_to_ptr(ptr_obj)
    };
    return MP_OBJ_FROM_PTR(self);
}

// Cast instance. Can be used in ISR when memory allocation is prohibited

STATIC inline mp_obj_t mp_lv_cast_instance(mp_obj_t self_in, mp_obj_t ptr_obj)
{
    mp_lv_struct_t *self = MP_OBJ_TO_PTR(self_in);
    self->data = mp_to_ptr(ptr_obj);
    return self_in;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_lv_cast_obj, mp_lv_cast);
STATIC MP_DEFINE_CONST_CLASSMETHOD_OBJ(mp_lv_cast_class_method, MP_ROM_PTR(&mp_lv_cast_obj));

STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_lv_cast_instance_obj, mp_lv_cast_instance);

// Dereference a struct/blob. This allows access to the raw data the struct holds

STATIC mp_obj_t mp_lv_dereference(size_t argc, const mp_obj_t *argv)
{
    mp_obj_t self_in = argv[0];
    mp_obj_t size_in = argc > 1? argv[1]: mp_const_none;
    mp_lv_struct_t *self = MP_OBJ_TO_PTR(self_in);
    size_t size = 0;
    if (size_in == mp_const_none){
        const mp_obj_type_t *type = self->base.type;
        size = get_lv_struct_size(type);
    } else {
        size = (size_t)mp_obj_get_int(size_in);
    }
    mp_obj_array_t *view = MP_OBJ_TO_PTR(mp_obj_new_memoryview(BYTEARRAY_TYPECODE,
        size, self->data));
    view->typecode |= 0x80; // used to indicate writable buffer
    return MP_OBJ_FROM_PTR(view);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_lv_dereference_obj, 1, 2, mp_lv_dereference);

// Callback function handling
// Callback is either a callable object or a pointer. If it's a callable object, set user_data to the callback.
// Multiple callbacks are kept per object/struct using a dict that associate callback name with callback object
// In case of an lv_obj_t, user_data is mp_lv_obj_t which contains a member "callbacks" for that dict.
// In case of a struct, user_data is a pointer to that dict directly

STATIC mp_obj_t get_callback_dict_from_user_data(void *user_data)
{
    if (user_data){
        mp_obj_t obj = MP_OBJ_FROM_PTR(user_data);
#ifdef LV_OBJ_T
        return 
            MP_OBJ_IS_TYPE(obj, &mp_type_dict)? obj: // Handle the case of dict for a struct
            mp_get_callbacks(obj); // Handle the case of mp_lv_obj_t for an lv_obj_t
#else
        return obj;
#endif
    }
    return NULL;
}

STATIC void *mp_lv_callback(mp_obj_t mp_callback, void *lv_callback, qstr callback_name, void **user_data_ptr)
{
    if (lv_callback && mp_obj_is_callable(mp_callback)){
        if (user_data_ptr){
            // user_data is either a dict of callbacks in case of struct, or a pointer to mp_lv_obj_t in case of lv_obj_t
            if (! (*user_data_ptr) ) *user_data_ptr = MP_OBJ_TO_PTR(mp_obj_new_dict(0)); // if it's NULL - it's a dict for a struct
            mp_obj_t callbacks = get_callback_dict_from_user_data(*user_data_ptr);
            mp_obj_dict_store(callbacks, MP_OBJ_NEW_QSTR(callback_name), mp_callback);
        }
        return lv_callback;
    } else {
        return mp_to_ptr(mp_callback);
    }
}

// Dict to hold user data for global callbacks (callbacks without context)



/*
 * LVGL string constants
 */


/*
 * Helper Structs
 */
        

typedef union {
    void*           ptr_val;
    const char*     str_val;
    int             int_val;
    unsigned int    uint_val;
    short           short_val[sizeof(void*) / sizeof(short)];
    unsigned short  ushort_val[sizeof(void*) / sizeof(unsigned short)];
    char            char_val[sizeof(void*) / sizeof(char)];
    unsigned char   uchar_val[sizeof(void*) / sizeof(unsigned char)];
} C_Pointer;


/*
 * Array convertors for short [(sizeof(void *)) / (sizeof(short))]
 */

STATIC short *mp_arr_to_short_____sizeof__void_ptr______div____sizeof__short______(mp_obj_t mp_arr)
{
    mp_obj_t mp_len = mp_obj_len_maybe(mp_arr);
    if (mp_len == MP_OBJ_NULL) return mp_to_ptr(mp_arr);
    mp_int_t len = mp_obj_get_int(mp_len);
    //TODO check dim!
    short *lv_arr = (short*)m_malloc(len * sizeof(short));
    mp_obj_t iter = mp_getiter(mp_arr, NULL);
    mp_obj_t item;
    size_t i = 0;
    while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        lv_arr[i++] = (short)mp_obj_get_int(item);
    }
    return (short *)lv_arr;
}
    
STATIC mp_obj_t mp_arr_from_short_____sizeof__void_ptr______div____sizeof__short______(short *arr)
{
    mp_obj_t obj_arr[(sizeof(void *)) / (sizeof(short))];
    for (size_t i=0; i<(sizeof(void *)) / (sizeof(short)); i++){
        obj_arr[i] = mp_obj_new_int(arr[i]);
    }
    return mp_obj_new_list((sizeof(void *)) / (sizeof(short)), obj_arr); // TODO: return custom iterable object!
}
    

/*
 * Array convertors for unsigned short [(sizeof(void *)) / (sizeof(unsigned short))]
 */

STATIC unsigned short *mp_arr_to_unsigned_short_____sizeof__void_ptr______div____sizeof__unsigned_short______(mp_obj_t mp_arr)
{
    mp_obj_t mp_len = mp_obj_len_maybe(mp_arr);
    if (mp_len == MP_OBJ_NULL) return mp_to_ptr(mp_arr);
    mp_int_t len = mp_obj_get_int(mp_len);
    //TODO check dim!
    unsigned short *lv_arr = (unsigned short*)m_malloc(len * sizeof(unsigned short));
    mp_obj_t iter = mp_getiter(mp_arr, NULL);
    mp_obj_t item;
    size_t i = 0;
    while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        lv_arr[i++] = (unsigned short)mp_obj_get_int(item);
    }
    return (unsigned short *)lv_arr;
}
    
STATIC mp_obj_t mp_arr_from_unsigned_short_____sizeof__void_ptr______div____sizeof__unsigned_short______(unsigned short *arr)
{
    mp_obj_t obj_arr[(sizeof(void *)) / (sizeof(unsigned short))];
    for (size_t i=0; i<(sizeof(void *)) / (sizeof(unsigned short)); i++){
        obj_arr[i] = mp_obj_new_int_from_uint(arr[i]);
    }
    return mp_obj_new_list((sizeof(void *)) / (sizeof(unsigned short)), obj_arr); // TODO: return custom iterable object!
}
    

/*
 * Array convertors for char [(sizeof(void *)) / (sizeof(char))]
 */

STATIC char *mp_arr_to_char_____sizeof__void_ptr______div____sizeof__char______(mp_obj_t mp_arr)
{
    mp_obj_t mp_len = mp_obj_len_maybe(mp_arr);
    if (mp_len == MP_OBJ_NULL) return mp_to_ptr(mp_arr);
    mp_int_t len = mp_obj_get_int(mp_len);
    //TODO check dim!
    char *lv_arr = (char*)m_malloc(len * sizeof(char));
    mp_obj_t iter = mp_getiter(mp_arr, NULL);
    mp_obj_t item;
    size_t i = 0;
    while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        lv_arr[i++] = (char)mp_obj_get_int(item);
    }
    return (char *)lv_arr;
}
    
STATIC mp_obj_t mp_arr_from_char_____sizeof__void_ptr______div____sizeof__char______(char *arr)
{
    mp_obj_t obj_arr[(sizeof(void *)) / (sizeof(char))];
    for (size_t i=0; i<(sizeof(void *)) / (sizeof(char)); i++){
        obj_arr[i] = mp_obj_new_int(arr[i]);
    }
    return mp_obj_new_list((sizeof(void *)) / (sizeof(char)), obj_arr); // TODO: return custom iterable object!
}
    

/*
 * Array convertors for unsigned char [(sizeof(void *)) / (sizeof(unsigned char))]
 */

STATIC unsigned char *mp_arr_to_unsigned_char_____sizeof__void_ptr______div____sizeof__unsigned_char______(mp_obj_t mp_arr)
{
    mp_obj_t mp_len = mp_obj_len_maybe(mp_arr);
    if (mp_len == MP_OBJ_NULL) return mp_to_ptr(mp_arr);
    mp_int_t len = mp_obj_get_int(mp_len);
    //TODO check dim!
    unsigned char *lv_arr = (unsigned char*)m_malloc(len * sizeof(unsigned char));
    mp_obj_t iter = mp_getiter(mp_arr, NULL);
    mp_obj_t item;
    size_t i = 0;
    while ((item = mp_iternext(iter)) != MP_OBJ_STOP_ITERATION) {
        lv_arr[i++] = (unsigned char)mp_obj_get_int(item);
    }
    return (unsigned char *)lv_arr;
}
    
STATIC mp_obj_t mp_arr_from_unsigned_char_____sizeof__void_ptr______div____sizeof__unsigned_char______(unsigned char *arr)
{
    mp_obj_t obj_arr[(sizeof(void *)) / (sizeof(unsigned char))];
    for (size_t i=0; i<(sizeof(void *)) / (sizeof(unsigned char)); i++){
        obj_arr[i] = mp_obj_new_int_from_uint(arr[i]);
    }
    return mp_obj_new_list((sizeof(void *)) / (sizeof(unsigned char)), obj_arr); // TODO: return custom iterable object!
}
    

/*
 * Struct C_Pointer
 */

STATIC inline const mp_obj_type_t *get_mp_C_Pointer_type();

STATIC inline C_Pointer* mp_write_ptr_C_Pointer(mp_obj_t self_in)
{
    mp_lv_struct_t *self = MP_OBJ_TO_PTR(cast(self_in, get_mp_C_Pointer_type()));
    return (C_Pointer*)self->data;
}

#define mp_write_C_Pointer(struct_obj) *mp_write_ptr_C_Pointer(struct_obj)

STATIC inline mp_obj_t mp_read_ptr_C_Pointer(C_Pointer *field)
{
    return lv_to_mp_struct(get_mp_C_Pointer_type(), (void*)field);
}

#define mp_read_C_Pointer(field) mp_read_ptr_C_Pointer(copy_buffer(&field, sizeof(C_Pointer)))
#define mp_read_byref_C_Pointer(field) mp_read_ptr_C_Pointer(&field)

STATIC void mp_C_Pointer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest)
{
    mp_lv_struct_t *self = MP_OBJ_TO_PTR(self_in);
    C_Pointer *data = (C_Pointer*)self->data;

    if (dest[0] == MP_OBJ_NULL) {
        // load attribute
        switch(attr)
        {
            case MP_QSTR_ptr_val: dest[0] = ptr_to_mp((void*)data->ptr_val); break; // converting from void *;
            case MP_QSTR_str_val: dest[0] = convert_to_str((void*)data->str_val); break; // converting from char *;
            case MP_QSTR_int_val: dest[0] = mp_obj_new_int(data->int_val); break; // converting from int;
            case MP_QSTR_uint_val: dest[0] = mp_obj_new_int_from_uint(data->uint_val); break; // converting from unsigned int;
            case MP_QSTR_short_val: dest[0] = mp_arr_from_short_____sizeof__void_ptr______div____sizeof__short______(data->short_val); break; // converting from short [(sizeof(void *)) / (sizeof(short))];
            case MP_QSTR_ushort_val: dest[0] = mp_arr_from_unsigned_short_____sizeof__void_ptr______div____sizeof__unsigned_short______(data->ushort_val); break; // converting from unsigned short [(sizeof(void *)) / (sizeof(unsigned short))];
            case MP_QSTR_char_val: dest[0] = mp_arr_from_char_____sizeof__void_ptr______div____sizeof__char______(data->char_val); break; // converting from char [(sizeof(void *)) / (sizeof(char))];
            case MP_QSTR_uchar_val: dest[0] = mp_arr_from_unsigned_char_____sizeof__void_ptr______div____sizeof__unsigned_char______(data->uchar_val); break; // converting from unsigned char [(sizeof(void *)) / (sizeof(unsigned char))];
            default: call_parent_methods(self_in, attr, dest); // fallback to locals_dict lookup
        }
    } else {
        if (dest[1])
        {
            // store attribute
            switch(attr)
            {
                case MP_QSTR_ptr_val: data->ptr_val = (void*)mp_to_ptr(dest[1]); break; // converting to void *;
                case MP_QSTR_str_val: data->str_val = (void*)(char*)mp_obj_str_get_str(dest[1]); break; // converting to char *;
                case MP_QSTR_int_val: data->int_val = (int)mp_obj_get_int(dest[1]); break; // converting to int;
                case MP_QSTR_uint_val: data->uint_val = (unsigned int)mp_obj_get_int(dest[1]); break; // converting to unsigned int;
                case MP_QSTR_short_val: memcpy(&data->short_val, mp_arr_to_short_____sizeof__void_ptr______div____sizeof__short______(dest[1]), sizeof(short)*(sizeof(void *)) / (sizeof(short))); break; // converting to short [(sizeof(void *)) / (sizeof(short))];
                case MP_QSTR_ushort_val: memcpy(&data->ushort_val, mp_arr_to_unsigned_short_____sizeof__void_ptr______div____sizeof__unsigned_short______(dest[1]), sizeof(unsigned short)*(sizeof(void *)) / (sizeof(unsigned short))); break; // converting to unsigned short [(sizeof(void *)) / (sizeof(unsigned short))];
                case MP_QSTR_char_val: memcpy(&data->char_val, mp_arr_to_char_____sizeof__void_ptr______div____sizeof__char______(dest[1]), sizeof(char)*(sizeof(void *)) / (sizeof(char))); break; // converting to char [(sizeof(void *)) / (sizeof(char))];
                case MP_QSTR_uchar_val: memcpy(&data->uchar_val, mp_arr_to_unsigned_char_____sizeof__void_ptr______div____sizeof__unsigned_char______(dest[1]), sizeof(unsigned char)*(sizeof(void *)) / (sizeof(unsigned char))); break; // converting to unsigned char [(sizeof(void *)) / (sizeof(unsigned char))];
                default: return;
            }

            dest[0] = MP_OBJ_NULL; // indicate success
        }
    }
}

STATIC void mp_C_Pointer_print(const mp_print_t *print,
    mp_obj_t self_in,
    mp_print_kind_t kind)
{
    mp_printf(print, "struct C_Pointer");
}

STATIC const mp_obj_dict_t mp_C_Pointer_locals_dict;

STATIC const mp_obj_type_t mp_C_Pointer_type = {
    { &mp_type_type },
    .name = MP_QSTR_C_Pointer,
    .print = mp_C_Pointer_print,
    .make_new = make_new_lv_struct,
    .attr = mp_C_Pointer_attr,
    .locals_dict = (mp_obj_dict_t*)&mp_C_Pointer_locals_dict,
    .buffer_p = { .get_buffer = mp_blob_get_buffer }
};

STATIC inline const mp_obj_type_t *get_mp_C_Pointer_type()
{
    return &mp_C_Pointer_type;
}
    
/* List of structs: ['C_Pointer'] */
/* Struct C_Pointer contains: [] */

STATIC const mp_rom_map_elem_t mp_C_Pointer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_SIZE), MP_ROM_PTR(MP_ROM_INT(sizeof(C_Pointer))) },
    { MP_ROM_QSTR(MP_QSTR_cast), MP_ROM_PTR(&mp_lv_cast_class_method) },
    { MP_ROM_QSTR(MP_QSTR_cast_instance), MP_ROM_PTR(&mp_lv_cast_instance_obj) },
    { MP_ROM_QSTR(MP_QSTR___dereference__), MP_ROM_PTR(&mp_lv_dereference_obj) },
    
};

STATIC MP_DEFINE_CONST_DICT(mp_C_Pointer_locals_dict, mp_C_Pointer_locals_dict_table);
        

/* 
 *
 * Global Module Functions
 *
 */


/*
 * ir extension definition for:
 * void rmtlib_nec_send(unsigned long data)
 */
 
STATIC mp_obj_t mp_rmtlib_nec_send(size_t mp_n_args, const mp_obj_t *mp_args)
{
    unsigned long data = (unsigned long)mp_obj_get_int(mp_args[0]);
    rmtlib_nec_send(data);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_LV_FUN_OBJ_VAR(mp_rmtlib_nec_send_obj, 1, mp_rmtlib_nec_send, rmtlib_nec_send);


/*
 * ir extension definition for:
 * void rmtlib_samsung_send(unsigned long data)
 */
 
STATIC mp_obj_t mp_rmtlib_samsung_send(size_t mp_n_args, const mp_obj_t *mp_args)
{
    unsigned long data = (unsigned long)mp_obj_get_int(mp_args[0]);
    rmtlib_samsung_send(data);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_LV_FUN_OBJ_VAR(mp_rmtlib_samsung_send_obj, 1, mp_rmtlib_samsung_send, rmtlib_samsung_send);


/*
 * ir extension definition for:
 * void rmtlib_sony_send(unsigned long data)
 */
 
STATIC mp_obj_t mp_rmtlib_sony_send(size_t mp_n_args, const mp_obj_t *mp_args)
{
    unsigned long data = (unsigned long)mp_obj_get_int(mp_args[0]);
    rmtlib_sony_send(data);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_LV_FUN_OBJ_VAR(mp_rmtlib_sony_send_obj, 1, mp_rmtlib_sony_send, rmtlib_sony_send);


/*
 * ir extension definition for:
 * void rmtlib_rc5_send(unsigned long data)
 */
 
STATIC mp_obj_t mp_rmtlib_rc5_send(size_t mp_n_args, const mp_obj_t *mp_args)
{
    unsigned long data = (unsigned long)mp_obj_get_int(mp_args[0]);
    rmtlib_rc5_send(data);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_LV_FUN_OBJ_VAR(mp_rmtlib_rc5_send_obj, 1, mp_rmtlib_rc5_send, rmtlib_rc5_send);


/*
 * ir extension definition for:
 * void rmtlib_raw_send(int frequency, int data[])
 */
 
STATIC mp_obj_t mp_rmtlib_raw_send(size_t mp_n_args, const mp_obj_t *mp_args)
{
    // retrieve frequency from mp_args
    unsigned int frequency = (unsigned int)mp_obj_get_int(mp_args[0]);

    // retrieve data from mp_args
    mp_uint_t data_len;
    mp_obj_t *data;
    mp_obj_get_array(mp_args[1], &data_len, &data);

    // call rmtlib_raw_send
    rmtlib_raw_send(frequency, (int *)data, (int)data_len);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_LV_FUN_OBJ_VAR(mp_rmtlib_raw_send_obj, 2, mp_rmtlib_raw_send, rmtlib_raw_send);



/*
 * ir module definitions
 */

STATIC const mp_rom_map_elem_t ir_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ir) },
    
    { MP_ROM_QSTR(MP_QSTR_rmtlib_nec_send), MP_ROM_PTR(&mp_rmtlib_nec_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmtlib_samsung_send), MP_ROM_PTR(&mp_rmtlib_samsung_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmtlib_sony_send), MP_ROM_PTR(&mp_rmtlib_sony_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmtlib_raw_send), MP_ROM_PTR(&mp_rmtlib_raw_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmtlib_rc5_send), MP_ROM_PTR(&mp_rmtlib_rc5_send_obj) }


};


STATIC MP_DEFINE_CONST_DICT (
    mp_module_ir_globals,
    ir_globals_table
);

const mp_obj_module_t mp_module_ir = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_ir_globals
};
MP_REGISTER_MODULE(MP_QSTR_ir, mp_module_ir, 1);

