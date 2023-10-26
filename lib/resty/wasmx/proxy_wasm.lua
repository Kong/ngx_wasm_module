-- vim:set ts=4 sw=4 sts=4 et:

local ffi = require "ffi"
local wasmx = require "resty.wasmx"


local C = ffi.C
local ngx = ngx
local error = error
local type = type
local pcall = pcall
local ffi_gc = ffi.gc
local ffi_new = ffi.new
local ffi_str = ffi.string
local tostring = tostring
local subsystem = ngx.config.subsystem
local get_request = wasmx.get_request
local get_err_ptr = wasmx.get_err_ptr
local FFI_OK = wasmx.FFI_OK
local FFI_ERROR = wasmx.FFI_ERROR
local FFI_ABORT = wasmx.FFI_ABORT
local FFI_DECLINED = wasmx.FFI_DECLINED
local NOT_FOUND = "missing"
local ERROR = "error"


local _M = {
    codes = {
        ERROR = ERROR,
        NOT_FOUND = NOT_FOUND,
    },
    isolations = {
        UNSET = 0,
        NONE = 1,
        STREAM = 2,
        FILTER = 3,
    }
}


if subsystem == "http" then
    ffi.cdef [[
        ngx_int_t ngx_http_wasm_ffi_plan_new(ngx_wasm_vm_t *vm,
                                             ngx_wasm_filter_t *filters,
                                             size_t n_filters,
                                             ngx_wasm_plan_t **out,
                                             u_char *err, size_t *errlen);
        ngx_int_t ngx_http_wasm_ffi_plan_free(ngx_wasm_plan_t *plan);
        ngx_int_t ngx_http_wasm_ffi_plan_load(ngx_wasm_plan_t *plan);
        ngx_int_t ngx_http_wasm_ffi_plan_attach(ngx_http_request_t *r,
                                                ngx_wasm_plan_t *plan,
                                                unsigned int isolation);
        ngx_int_t ngx_http_wasm_ffi_start(ngx_http_request_t *r);
        ngx_int_t ngx_http_wasm_ffi_set_property(ngx_http_request_t *r,
                                                 ngx_str_t *key,
                                                 ngx_str_t *value);
        ngx_int_t ngx_http_wasm_ffi_get_property(ngx_http_request_t *r,
                                                 ngx_str_t *key,
                                                 ngx_str_t *out);
        ngx_int_t ngx_http_wasm_ffi_set_host_property(ngx_http_request_t *r,
                                                      ngx_str_t *key,
                                                      ngx_str_t *value,
                                                      unsigned is_const,
                                                      unsigned retrieve);
        ngx_int_t ngx_http_wasm_ffi_set_property_setter(ngx_http_request_t *r,
                                                        ngx_wasm_host_prop_fn_t fn);
        ngx_int_t ngx_http_wasm_ffi_set_property_getter(ngx_http_request_t *r,
                                                        ngx_wasm_host_prop_fn_t fn);
    ]]

else
    -- Only support the HTTP subsystem for now.
    return _M
end


local ffi_ops_type = ffi.typeof("ngx_wasm_plan_t *")
local ffi_pops_type = ffi.typeof("ngx_wasm_plan_t *[1]")


local lua_prop_setter
local lua_prop_getter
local c_prop_setter
local c_prop_getter


do
    -- LuaJIT C callbacks are a limited resource:
    -- We can't create one of these on each request,
    -- so we prepare these wrappers, which in turn call a Lua function.

    local function store_c_value(r, ckey, cvalue, lvalue, is_const)
        if lvalue ~= nil then
            local value = tostring(lvalue)
            cvalue.data = value
            cvalue.len = #value

        else
            cvalue.data = nil
            cvalue.len = 0
        end

        return C.ngx_http_wasm_ffi_set_host_property(r, ckey, cvalue,
                                                     is_const and 1 or 0,
                                                     1)
    end

    c_prop_setter = ffi.cast("ngx_wasm_host_prop_fn_t", function(r, ckey,
                                                                 cvalue)
        local lkey = ffi_str(ckey.data, ckey.len)
        local lval
        if cvalue.data ~= nil then
            lval = ffi_str(cvalue.data, cvalue.len)
        end

        jit.off()
        local pok, ok, lvalue, is_const = pcall(lua_prop_setter, lkey, lval)
        jit.on()
        if not pok then
            ngx.log(ngx.ERR, "error setting property from Lua: ", ok)
            return FFI_ERROR
        end

        if not ok then
            local err = lvalue or "unknown error"
            ngx.log(ngx.ERR, "error setting property: ", err)
            return FFI_ERROR
        end

        return store_c_value(r, ckey, cvalue, lvalue, is_const)
    end)


    c_prop_getter = ffi.cast("ngx_wasm_host_prop_fn_t", function(r, ckey,
                                                                 cvalue)
        local lkey = ffi_str(ckey.data, ckey.len)

        jit.off()
        local pok, ok, lvalue, is_const = pcall(lua_prop_getter, lkey)
        jit.on()
        if not pok then
            ngx.log(ngx.ERR, "error getting property from Lua: ", ok)
            return FFI_ERROR
        end

        if not ok then
            if lvalue then
                ngx.log(ngx.ERR, "error setting property: ", lvalue)
                return FFI_ERROR
            end

            return FFI_DECLINED
        end

        local rc = store_c_value(r, ckey, cvalue, lvalue, is_const)
        if rc == FFI_OK and lvalue == nil then
            return FFI_DECLINED
        end

        return rc
    end)
end


function _M.new(filters)
    if type(filters) ~= "table" then
        error("filters must be a table", 2)
    end

    local vm = wasmx.get_main_vm()
    if vm == nil then
        error("no wasm vm", 2)
    end

    local nfilters = #filters
    local cfilters = ffi_new("ngx_wasm_filter_t[?]", nfilters)

    for i = 1, nfilters do
        local filter = filters[i]

        if type(filter) ~= "table" then
            error("filter[" .. i .. "] must be a table", 2)
        end

        local name = filter.name
        local config = filter.config

        if type(name) ~= "string" then
            error("filter[" .. i .. "].name must be a string", 2)
        end

        if config == nil then
            config = ""

        elseif type(config) ~= "string" then
            error("filter[" .. i .. "].config should be a string", 2)
        end

        local cname = ffi_new("ngx_str_t", { data = name, len = #name })
        local cconf = ffi_new("ngx_str_t", { data = config, len = #config })

        local cfilter = cfilters[i - 1]
        cfilter.name = ffi.cast("ngx_str_t *", cname)
        cfilter.config = ffi.cast("ngx_str_t *", cconf)
    end

    local plan = ffi_new(ffi_ops_type)
    local pplan = ffi_new(ffi_pops_type, plan)
    local pcfilters = ffi.cast("ngx_wasm_filter_t *", cfilters)
    local errbuf, errlen = get_err_ptr()

    local rc = C.ngx_http_wasm_ffi_plan_new(vm, pcfilters, nfilters,
                                            pplan, errbuf, errlen)
    if rc == FFI_ERROR then
        return nil, ffi_str(errbuf, errlen[0])
    end

    if rc == FFI_OK then
        local c_plan = pplan[0]

        return ffi_gc(c_plan, C.ngx_http_wasm_ffi_plan_free)
    end

    return nil, "bad ffi rc: " .. rc
end


function _M.load(c_plan)
    if type(c_plan) ~= "cdata" then
        error("plan should be a cdata object", 2)
    end

    local phase = ngx.get_phase()
    if phase == "init" then
        error("load cannot be called from 'init' phase", 2)
    end

    local rc = C.ngx_http_wasm_ffi_plan_load(c_plan)
    if rc == FFI_ERROR then
        return nil, "failed loading plan"
    end

    return true
end


function _M.attach(c_plan, opts)
    local isolation = _M.isolations.UNSET

    if type(c_plan) ~= "cdata" then
        error("plan must be a cdata object", 2)
    end

    if opts ~= nil then
        if type(opts) ~= "table" then
            error("opts must be a table", 2)
        end

        if opts.isolation ~= nil then
            if opts.isolation ~= _M.isolations.NONE
               and opts.isolation ~= _M.isolations.STREAM
               and opts.isolation ~= _M.isolations.FILTER
            then
                error("bad opts.isolation value: " .. opts.isolation, 2)
            end

            isolation = opts.isolation
        end
    end

    local phase = ngx.get_phase()
    if phase ~= "rewrite" and phase ~= "access" then
        error("attach must be called from 'rewrite' or 'access' phase", 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    local rc = C.ngx_http_wasm_ffi_plan_attach(r, c_plan, isolation)
    if rc == FFI_ERROR then
        return nil, "unknown error"
    end

    if rc == FFI_DECLINED then
        return nil, "plan not loaded"
    end

    if rc == FFI_ABORT then
        return nil, "previous plan already attached"
    end

    return true
end


function _M.start()
    local phase = ngx.get_phase()
    if phase ~= "rewrite" and phase ~= "access" then
        error("start must be called from 'rewrite' or 'access' phase", 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    local rc = C.ngx_http_wasm_ffi_start(r)
    if rc == FFI_ERROR then
        return nil, "unknown error"
    end

    if rc == FFI_DECLINED then
        return nil, "plan not loaded and attached"
    end

    return true
end


function _M.set_property(key, value)
    if type(key) ~= "string" then
        error("key must be a string", 2)
    end

    if value ~= nil and type(value) ~= "string" then
        error("value must be a string", 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    local ckey = ffi_new("ngx_str_t", { data = key, len = #key })
    local cvalue = ffi_new("ngx_str_t", { data = value, len = value and #value or 0 })

    local rc = C.ngx_http_wasm_ffi_set_property(r, ckey, cvalue)
    if rc == FFI_ERROR then
        return nil, "unknown error"
    end

    if rc == FFI_DECLINED then
        return nil, "could not set property"
    end

    return true
end


function _M.get_property(key)
    if type(key) ~= "string" then
        error("key must be a string", 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    local ckey = ffi_new("ngx_str_t", { data = key, len = #key })
    local cvalue = ffi_new("ngx_str_t", { data = nil, len = 0 })

    local rc = C.ngx_http_wasm_ffi_get_property(r, ckey, cvalue)
    if rc == FFI_ERROR then
        return nil, "unknown error", ERROR
    end

    if rc == FFI_DECLINED then
        return nil, "property \"" .. key .. "\" not found", NOT_FOUND
    end

    return ffi_str(cvalue.data, cvalue.len)
end


---
-- Define a setter function for host-managed properties.
--
-- @param setter The setter function is the handler for updating properties.
-- Its type signature is `function(string, string): boolean, string`,
-- where the inputs are the property key and value and the results are:
-- * `truthy` - success setting the property
-- * `truthy, value` - success, cache property value (useful if hosts
--   sets only a setter but not a getter)
-- * `truthy, value, truthy` - success, constant property value: further
--   requests to the same property during a request are cached by
--   ngx_wasm_module and do not invoke the Lua getter.
-- * `falsy, err` - failure, and an optional error message
-- @return true on success setting the getter, or nil and an error message.
function _M.set_property_setter(setter)
    if type(setter) ~= "function" then
        error("setter must be a function", 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    lua_prop_setter = setter

    local rc = C.ngx_http_wasm_ffi_set_property_setter(r, c_prop_setter)
    if rc == FFI_OK then
        return true
    end

    return nil, "could not set property setter"
end


---
-- Define a getter function for host-managed properties.
--
-- @param getter The getter function is the handler for resolving properties.
-- Its type signature is `function(string): boolean, string, boolean`,
-- where the input is the property key and the results are:
-- * `truthy, value` - success, property value
-- * `truthy, value, truthy` - success, constant property value: further
--   requests to the same property during a request are cached by
--   ngx_wasm_module and do not invoke the Lua getter.
-- * `falsy` - property not found
-- * `falsy, err` - failure, error message
-- @return true on success setting the getter, or nil and an error message.
function _M.set_property_getter(getter)
    if type(getter) ~= "function" then
        error("getter must be a function", 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    lua_prop_getter = getter

    local rc = C.ngx_http_wasm_ffi_set_property_getter(r, c_prop_getter)
    if rc == FFI_OK then
        return true
    end

    return nil, "could not set property getter"
end


return _M
