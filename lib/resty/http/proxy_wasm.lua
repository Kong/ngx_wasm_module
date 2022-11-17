-- vim:set st=4 sw=4 sts=4 et:

local ffi = require "ffi"
local wasm = require "resty.wasm"


local C = ffi.C
local error = error
local type = type
local ffi_gc = ffi.gc
local ffi_new = ffi.new
local ffi_str = ffi.string
local get_request = wasm.get_request
local get_err_ptr = wasm.get_err_ptr
local FFI_OK = wasm.FFI_OK
local FFI_ERROR = wasm.FFI_ERROR
local FFI_DECLINED = wasm.FFI_DECLINED


ffi.cdef [[
    int ngx_http_wasm_ffi_plan_new(ngx_wasm_vm_t *vm,
                                   ngx_wasm_filter_t *filters,
                                   size_t n_filters,
                                   ngx_wasm_plan_t **out,
                                   u_char *err, size_t *errlen);
    int ngx_http_wasm_ffi_plan_free(ngx_wasm_plan_t *plan);
    int ngx_http_wasm_ffi_plan_load(ngx_wasm_plan_t *plan);
    int ngx_http_wasm_ffi_plan_attach(ngx_http_request_t *r,
                                      ngx_wasm_plan_t *plan);
]]


local ffi_ops_type = ffi.typeof("ngx_wasm_plan_t *")
local ffi_pops_type = ffi.typeof("ngx_wasm_plan_t *[1]")


local _M = {}


function _M.new(filters)
    if type(filters) ~= "table" then
        error("filters must be a table", 2)
    end

    local vm = wasm.get_main_vm()
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

    local rc = C.ngx_http_wasm_ffi_plan_new(vm, pcfilters, nfilters, pplan,
                                            errbuf, errlen)
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

    local rc = C.ngx_http_wasm_ffi_plan_load(c_plan)
    if rc == FFI_ERROR then
        return nil, "unknown error"
    end

    return true
end


function _M.attach(c_plan)
    if type(c_plan) ~= "cdata" then
        error("plan should be a cdata object", 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    local rc = C.ngx_http_wasm_ffi_plan_attach(r, c_plan)
    if rc == FFI_ERROR then
        return nil, "unknown error"
    end

    if rc == FFI_DECLINED then
        return nil, "plan not loaded"
    end

    return true
end


return _M
