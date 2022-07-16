-- vim:set st=4 sw=4 sts=4 et:

local ffi = require "ffi"
local wasm = require "resty.wasm"
local base = require "resty.core.base"
local tablepool
do
    local pok
    pok, tablepool = pcall(require, "tablepool")
    if not pok then
        -- fallback for OpenResty < 1.15.8.1
        tablepool = {
            fetch = function(_, narr, nrec)
                return new_tab(narr, nrec)
            end,
            release = function(_, _, _)
                -- nop (obj will be subject to GC)
            end,
        }
    end
end


local C = ffi.C
local error = error
local type = type
local find = string.find
local ffi_gc = ffi.gc
local ffi_new = ffi.new
local ffi_str = ffi.string
local ngx_phase = ngx.get_phase
local get_request = base.get_request
local FFI_DECLINED = base.FFI_DECLINED
local FFI_ERROR = base.FFI_ERROR
local FFI_DONE = base.FFI_DONE
local FFI_OK = base.FFI_OK


ffi.cdef [[
    int ngx_http_wasm_ffi_pwm_new(ngx_wasm_vm_t *vm,
                                  ngx_wasm_filter_t *filters,
                                  size_t n_filters,
                                  ngx_wasm_ops_t **out);
    int ngx_http_wasm_ffi_pwm_resume(ngx_http_request_t *r,
                                     ngx_wasm_ops_t *ops,
                                     unsigned int phase);
    void ngx_http_wasm_ffi_pwm_free(ngx_wasm_ops_t *ops);
]]


local ffi_ops_type = ffi.typeof("ngx_wasm_ops_t *")
local ffi_pops_type = ffi.typeof("ngx_wasm_ops_t *[1]")
--local ffi_filter_type = ffi.typeof("ngx_wasm_filter_t")


local _M = {}


function _M.new(filters, no_gc)
    if type(filters) ~= "table" then
        error("filters must be a table", 2)
    end

    local phase = ngx_phase()

    if phase ~= "init_worker" and phase ~= "access" then
        error([[must be called from "init_worker" or "access"]], 2)
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

    local ops = ffi_new(ffi_ops_type)
    local pops = ffi_new(ffi_pops_type, ops)
    local pcfilters = ffi.cast("ngx_wasm_filter_t *", cfilters)

    local rc = C.ngx_http_wasm_ffi_pwm_new(vm, pcfilters, nfilters, pops)
    if rc == FFI_ERROR then
        return nil, "unknown error"
    end

    if rc == FFI_DECLINED then
        return nil, "no wasm vm"
    end

    if rc == FFI_OK or rc == FFI_DONE then
        local c_ops = pops[0]

        if rc == FFI_OK and not no_gc then
            c_ops = ffi_gc(c_ops, C.ngx_http_wasm_ffi_pwm_free)
        end

        return c_ops
    end

    return nil, "bad ffi rc: " .. rc
end


function _M.attach(root_c_ops, filters)
    if ngx_phase() ~= "access" then
        error([[must be called from "access"]], 2)
    end

    local r = get_request()
    if not r then
        error("no request found", 2)
    end

    local HTTP_REWRITE_PHASE = 3
    local HTTP_CONTENT_PHASE = 9

    local rc = C.ngx_http_wasm_ffi_pwm_resume(r, c_ops, HTTP_REWRITE_PHASE)
    if rc ~= FFI_OK then
        return nil, "error"
    end

    rc = C.ngx_http_wasm_ffi_pwm_resume(r, c_ops, HTTP_CONTENT_PHASE)
    if rc ~= FFI_OK then
        return nil, "error"
    end

    return true
end


return _M
