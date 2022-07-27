#include <JavaScriptCore/JavaScript.h>
#include <stdlib.h>
#include "wasm.h"

struct wasm_engine_t
{
    JSContextGroupRef group;
};

struct wasm_store_t
{
    wasm_engine_t *engine;
    JSGlobalContextRef context;
    JSObjectRef global;
    JSObjectRef webassembly_namespace;
    JSObjectRef module_constructor;
    JSObjectRef instance_constructor;
};

struct wasm_module_t
{
    JSGlobalContextRef context;
    JSObjectRef object;
};

static void dummy_dealloc(void *bytes, void *ctx)
{
}

wasm_engine_t *wasm_engine_new()
{
    return wasm_engine_new_with_config(NULL);
}

wasm_engine_t *wasm_engine_new_with_config(wasm_config_t *config)
{
    wasm_engine_t *engine;

    engine = malloc(sizeof(wasm_engine_t));
    if (!engine)
        return NULL;

    engine->group = JSContextGroupCreate();
    if (!engine->group)
    {
        free(engine);
        return NULL;
    }
    return engine;
}

void wasm_engine_delete(wasm_engine_t *engine)
{
    if (!engine)
        return;
    JSContextGroupRelease(engine->group);
    free(engine);
}

wasm_store_t *wasm_store_new(wasm_engine_t *engine)
{
    wasm_store_t *store;
    JSStringRef strbuf;

    store = malloc(sizeof(wasm_store_t));
    if (!store)
    {
        return NULL;
    }

    store->engine = engine;
    store->context = JSGlobalContextCreateInGroup(engine->group, NULL);
    if (!store->context)
    {
        free(store);
        return NULL;
    }

    store->global = JSContextGetGlobalObject(store->context);
    assert(store->global);

    strbuf = JSStringCreateWithUTF8CString("WebAssembly");
    store->webassembly_namespace = JSValueToObject(store->context, JSObjectGetProperty(store->context, store->global, strbuf, NULL), NULL);
    assert(store->webassembly_namespace);
    JSStringRelease(strbuf);

    strbuf = JSStringCreateWithUTF8CString("Module");
    store->module_constructor = JSValueToObject(store->context, JSObjectGetProperty(store->context, store->webassembly_namespace, strbuf, NULL), NULL);
    assert(store->module_constructor);
    JSStringRelease(strbuf);

    strbuf = JSStringCreateWithUTF8CString("Instance");
    store->instance_constructor = JSValueToObject(store->context, JSObjectGetProperty(store->context, store->webassembly_namespace, strbuf, NULL), NULL);
    assert(store->instance_constructor);
    JSStringRelease(strbuf);

    return store;
}

void wasm_store_delete(wasm_store_t *store)
{
    if (!store)
        return;
    JSGlobalContextRelease(store->context);
    free(store);
}

struct wasm_valtype_t
{
    wasm_valkind_t kind;
};

wasm_valtype_t *wasm_valtype_new(wasm_valkind_t kind)
{
    wasm_valtype_t *valtype;
    valtype = malloc(sizeof(wasm_valtype_t));
    if (!valtype)
        return NULL;
    valtype->kind = kind;
    return valtype;
}

wasm_valkind_t wasm_valtype_kind(const wasm_valtype_t *vt)
{
    return vt->kind;
}

void wasm_valtype_vec_new_empty(wasm_valtype_vec_t *vec)
{
    vec->size = 0;
    vec->data = NULL;
}

void wasm_valtype_vec_new_uninitialized(wasm_valtype_vec_t *out, size_t size)
{
    out->size = size;
    out->data = malloc(sizeof(wasm_valtype_t) * size);
    assert(out->data);
}

struct wasm_functype_t
{
    wasm_valtype_vec_t *param_types;
    wasm_valtype_vec_t *result_types;
};

wasm_functype_t *wasm_functype_new(wasm_valtype_vec_t *params, wasm_valtype_vec_t *results)
{
    wasm_functype_t *functype;
    functype = malloc(sizeof(wasm_functype_t));
    if (!functype)
        return NULL;
    functype->param_types = params;
    functype->result_types = results;
    return functype;
}

wasm_module_t *wasm_module_new(wasm_store_t *store, const wasm_byte_vec_t *binary)
{
    wasm_module_t *module;
    JSValueRef exception, buffer;

    module = malloc(sizeof(wasm_module_t));
    if (!module)
        return NULL;

    module->context = store->context;
    buffer = JSObjectMakeTypedArrayWithBytesNoCopy(store->context, kJSTypedArrayTypeUint8Array, binary->data, binary->size, dummy_dealloc, NULL, NULL);
    module->object = JSObjectCallAsConstructor(store->context, store->module_constructor, 1, &buffer, &exception);

    if (exception)
    {
        free(module);
        return NULL;
    }
    assert(module->object);
    JSValueProtect(store->context, module->object);
    return module;
}

void wasm_module_delete(wasm_module_t *module)
{
    JSValueUnprotect(module->context, module->object);
    free(module);
}

bool wasm_module_validate(wasm_store_t *store, const wasm_byte_vec_t *binary)
{
    JSObjectRef validate_func;
    JSValueRef result, buffer;
    JSStringRef validate_str;

    validate_str = JSStringCreateWithUTF8CString("validate");
    validate_func = JSValueToObject(store->context, JSObjectGetProperty(store->context, store->webassembly_namespace, validate_str, NULL), NULL);
    assert(validate_func);
    JSStringRelease(validate_str);

    buffer = JSObjectMakeTypedArrayWithBytesNoCopy(store->context, kJSTypedArrayTypeUint8Array, binary->data, binary->size, dummy_dealloc, NULL, NULL);
    result = JSObjectCallAsFunction(store->context, validate_func, store->webassembly_namespace, 1, &buffer, NULL);
    return JSValueToBoolean(store->context, result);
}

void wasm_module_imports(const wasm_module_t *module, wasm_importtype_vec_t *out)
{
    assert(0);
}

void wasm_module_exports(const wasm_module_t *module, wasm_exporttype_vec_t *out)
{
    assert(0);
}

wasm_instance_t *wasm_instance_new(wasm_store_t *store, const wasm_module_t *module, const wasm_extern_vec_t *imports, wasm_trap_t **trap)
{
    assert(0);
}

void wasm_instance_exports(const wasm_instance_t *instance, wasm_extern_vec_t *out)
{
    assert(0);
}

void wasm_instance_delete(wasm_instance_t *inst)
{
    assert(0);
}

void wasm_byte_vec_new(wasm_byte_vec_t *out, size_t len, const wasm_byte_t *data)
{
    out->data = malloc(len);
    assert(out->data);

    out->size = len;
    memcpy(out->data, data, len);
    /*
    func JSObjectMakeTypedArrayWithBytesNoCopy(
    _ ctx: JSContextRef!,
    _ arrayType: JSTypedArrayType,
    _ bytes: UnsafeMutableRawPointer!,
    _ byteLength: Int,
    _ bytesDeallocator: JSTypedArrayBytesDeallocator!,
    _ deallocatorContext: UnsafeMutableRawPointer!,
    _ exception: UnsafeMutablePointer<JSValueRef?>!
) -> JSObjectRef!
    */
}

void wasm_byte_vec_delete(wasm_byte_vec_t *x)
{
    if (x->data)
        free(x->data);
}

void wasm_byte_vec_new_uninitialized(wasm_byte_vec_t *out, size_t len)
{
    out->data = malloc(len);
    assert(out->data);

    out->size = len;
}

void wasm_config_delete(wasm_config_t *config)
{
    assert(0);
}

wasm_config_t *wasm_config_new()
{
    assert(0);
}
const wasm_name_t *wasm_exporttype_name(const wasm_exporttype_t *ty)
{
    assert(0);
}
const wasm_externtype_t *wasm_exporttype_type(const wasm_exporttype_t *ty)
{
    assert(0);
}
void wasm_exporttype_vec_delete(wasm_exporttype_vec_t *vec)
{
    assert(0);
}

wasm_func_t *wasm_extern_as_func(wasm_extern_t *ext)
{
    assert(0);
}
wasm_memory_t *wasm_extern_as_memory(wasm_extern_t *ext)
{
    assert(0);
}
wasm_externkind_t wasm_extern_kind(const wasm_extern_t *ext)
{
    assert(0);
}
wasm_externkind_t wasm_externtype_kind(const wasm_externtype_t *ty)
{
    assert(0);
}

void wasm_extern_vec_delete(wasm_extern_vec_t *vec)
{
    assert(0);
}
wasm_extern_t *wasm_func_as_extern(wasm_func_t *func)
{
    assert(0);
}

wasm_trap_t *wasm_func_call(
    const wasm_func_t *func, const wasm_val_vec_t *args, wasm_val_vec_t *results)
{
    assert(0);
}

void wasm_func_delete(wasm_func_t *func)
{
    assert(0);
}

wasm_func_t *wasm_func_new_with_env(
    wasm_store_t *store, const wasm_functype_t *type, wasm_func_callback_with_env_t callback,
    void *env, void (*finalizer)(void *))
{
    assert(0);
}

wasm_functype_t *wasm_func_type(const wasm_func_t *func)
{
    assert(0);
}

void wasm_functype_delete(wasm_functype_t *ty)
{
    assert(0);
}
const wasm_valtype_vec_t *wasm_functype_params(const wasm_functype_t *ty)
{
    assert(0);
}
const wasm_valtype_vec_t *wasm_functype_results(const wasm_functype_t *ty)
{
    assert(0);
}
const wasm_name_t *wasm_importtype_module(const wasm_importtype_t *ty)
{
    assert(0);
}
const wasm_name_t *wasm_importtype_name(const wasm_importtype_t *ty)
{
    assert(0);
}
const wasm_externtype_t *wasm_importtype_type(const wasm_importtype_t *ty)
{
    assert(0);
}