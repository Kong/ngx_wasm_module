-- vim:set st=4 sw=4 sts=4 et:

local ffi = require "ffi"


local C = ffi.C
local error = error
local type = type


ffi.cdef [[
    unsigned int ngx_proxy_wasm_filter_id(ngx_str_t *name,
                                          ngx_str_t *config,
                                          ngx_uint_t idx);
]]


local name_str_t = ffi.new("ngx_str_t[1]")
local config_str_t = ffi.new("ngx_str_t[1]")


local _M = {}


function _M.get_filter_id(name, config, index)
    if type(name) ~= "string" then
        error("name must be a string", 2)
    end

    if type(config) ~= "string" then
        error("config must be a string", 2)
    end

    if type(index) ~= "number" then
        error("index must be a number", 2)
    end

    local name_str_t = name_str_t[0]
    local config_str_t = config_str_t[0]

    name_str_t.data = name
    name_str_t.len = #name

    config_str_t.data = config
    config_str_t.len = #config

    return C.ngx_proxy_wasm_filter_id(name_str_t, config_str_t, index)
end
