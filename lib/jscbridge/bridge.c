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
};

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
    JSContextGroupRetain(engine->group);
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
    JSGlobalContextRetain(store->context);
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
