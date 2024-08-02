-- vim:set ts=4 sw=4 sts=4 et:

local ffi = require "ffi"
local wasmx = require "resty.wasmx"


local C = ffi.C
local error = error
local type = type
local table_new = table.new
local ffi_cast = ffi.cast
local ffi_fill = ffi.fill
local ffi_new = ffi.new
local ffi_str = ffi.string
local ngx_log = ngx.log
local initialized = false
local FFI_DECLINED = wasmx.FFI_DECLINED
local FFI_ERROR = wasmx.FFI_ERROR
local WASM_SHM_KEY = {}  -- ensures shm is only locally accessible

local get_zones_handler

local types = {
    ffi_shm = {
        SHM_TYPE_KV = 0,
        SHM_TYPE_QUEUE = 1,
        SHM_TYPE_METRICS = 2,
    },
    ffi_metric = {
        COUNTER = 0,
        GAUGE = 1,
        HISTOGRAM = 2,
    }
}

local _M = {}


ffi.cdef [[
    typedef unsigned char           u_char;
    typedef uintptr_t               ngx_uint_t;
    typedef ngx_uint_t              ngx_msec_t;
    typedef struct ngx_log_s        ngx_log_t;
    typedef struct ngx_slab_pool_s  ngx_slab_pool_t;

    typedef enum {
        NGX_WASM_SHM_TYPE_KV,
        NGX_WASM_SHM_TYPE_QUEUE,
        NGX_WASM_SHM_TYPE_METRICS,
    } ngx_wasm_shm_type_e;

    typedef enum {
        NGX_WASM_SHM_EVICTION_LRU,
        NGX_WASM_SHM_EVICTION_SLRU,
        NGX_WASM_SHM_EVICTION_NONE,
    } ngx_wasm_shm_eviction_e;

    typedef struct {
        ngx_wasm_shm_type_e       type;
        ngx_wasm_shm_eviction_e   eviction;
        ngx_str_t                 name;
        ngx_log_t                *log;
        ngx_slab_pool_t          *shpool;
        void                     *data;
    } ngx_wasm_shm_t;

    typedef enum {
        NGX_WA_METRIC_COUNTER,
        NGX_WA_METRIC_GAUGE,
        NGX_WA_METRIC_HISTOGRAM,
    } ngx_wa_metric_type_e;


    typedef struct {
        ngx_uint_t                   value;
        ngx_msec_t                   last_update;
    } ngx_wa_metrics_gauge_t;

    typedef struct {
        uint32_t                     upper_bound;
        uint32_t                     count;
    } ngx_wa_metrics_bin_t;

    typedef struct {
        uint8_t                      n_bins;
        ngx_wa_metrics_bin_t         bins[];
    } ngx_wa_metrics_histogram_t;

    typedef union {
        ngx_uint_t                   counter;
        ngx_wa_metrics_gauge_t       gauge;
        ngx_wa_metrics_histogram_t  *histogram;
    } ngx_wa_metric_val_t;

    typedef struct {
        ngx_wa_metric_type_e         metric_type;
        ngx_wa_metric_val_t          slots[];
    } ngx_wa_metric_t;


    typedef void (*ngx_wa_ffi_shm_get_zones_handler_pt)(ngx_wasm_shm_t *shm);


    int ngx_wa_ffi_shm_get_zones(ngx_wa_ffi_shm_get_zones_handler_pt handler);
    int ngx_wa_ffi_shm_get_keys(ngx_wasm_shm_t *shm,
                                ngx_uint_t n,
                                ngx_str_t **keys);
    int ngx_wa_ffi_shm_get_kv_value(ngx_wasm_shm_t *shm,
                                    ngx_str_t *k,
                                    ngx_str_t **v,
                                    uint32_t *cas);
    int ngx_wa_ffi_shm_get_metric(ngx_str_t *k,
                                  u_char *mbuf, size_t mbs,
                                  u_char *hbuf, size_t hbs);
    int ngx_wa_ffi_shm_one_slot_metric_size();
    int ngx_wa_ffi_shm_max_histogram_size();
]]


local function get_keys(zone, max)
    if max ~= nil and (type(max) ~= "number" or max < 1) then
        error("n must be a positive number", 2)
    end

    local shm = zone[WASM_SHM_KEY]
    local nkeys = C.ngx_wa_ffi_shm_get_keys(shm, 0, nil)
    if nkeys == FFI_ERROR then
        return nil, "failed retrieving shm zone keys total"
    end

    nkeys = (max and nkeys > max) and max or nkeys

    local ckeys = ffi_new("ngx_str_t *[?]", nkeys)
    local pckeys = ffi_cast("ngx_str_t **", ckeys)
    local rc = C.ngx_wa_ffi_shm_get_keys(shm, nkeys, pckeys)
    if rc == FFI_ERROR then
        return nil, "failed retrieving shm zone keys"
    end

    local keys = table_new(0, nkeys)

    for i = 1, nkeys do
        local ckey = ckeys[i - 1]
        keys[i] = ffi_str(ckey.data, ckey.len)
    end

    return keys
end


local function get_kv_value(zone, key)
    if type(key) ~= "string" then
        error("key must be a string", 2)
    end

    local shm = zone[WASM_SHM_KEY]
    local cname = ffi_new("ngx_str_t", { data = key, len = #key })
    local cvalue = ffi_new("ngx_str_t *[1]")
    local ccas = ffi_new("uint32_t [1]")

    local rc = C.ngx_wa_ffi_shm_get_kv_value(shm, cname, cvalue, ccas)
    if rc == FFI_ERROR then
        return nil, "failed retrieving kv value"
    end

    if rc == FFI_DECLINED then
        return nil, "key not found"
    end

    return ffi_str(cvalue[0].data, cvalue[0].len)
end


local function new_zero_filled_buffer(ctype, size)
    local buf = ffi_new(ctype, size)
    ffi_fill(buf, size)

    return buf
end


local function get_metric(zone, name)
    if type(name) ~= "string" then
        error("name must be a string", 2)
    end

    local cname = ffi_new("ngx_str_t", { data = name, len = #name })
    local mbs = C.ngx_wa_ffi_shm_one_slot_metric_size()
    local hbs = C.ngx_wa_ffi_shm_max_histogram_size()
    local mbuf = new_zero_filled_buffer("u_char [?]", mbs)
    local hbuf = new_zero_filled_buffer("u_char [?]", hbs)

    local rc = C.ngx_wa_ffi_shm_get_metric(cname, mbuf, mbs, hbuf, hbs)
    if rc == FFI_ERROR then
        return nil, "failed retrieving metric"
    end

    if rc == FFI_DECLINED then
        return nil, "metric not found"
    end

    local cmetric = ffi_cast("ngx_wa_metric_t *", mbuf)

    if cmetric.metric_type == types.ffi_metric.COUNTER then
        return { type = "counter", value = tonumber(cmetric.slots[0].counter) }

    elseif cmetric.metric_type == types.ffi_metric.GAUGE then
        return { type = "gauge", value = tonumber(cmetric.slots[0].gauge.value) }

    elseif cmetric.metric_type == types.ffi_metric.HISTOGRAM then
        local ch = ffi_cast("ngx_wa_metrics_histogram_t *", hbuf)
        local h = { type = "histogram", value = {} }

        for i = 0, ch.n_bins do
            local cb = ch.bins[i]

            if cb.upper_bound == 0 then
                break
            end

            h.value[cb.upper_bound] = cb.count
        end

        return h
    end
end


local get_zones_handler = ffi_cast("ngx_wa_ffi_shm_get_zones_handler_pt",
function(shm)
    local zone_name = ffi_str(shm.name.data, shm.name.len)
    _M[zone_name] = { [WASM_SHM_KEY] = shm }

    if shm.type == types.ffi_shm.SHM_TYPE_KV then
        _M[zone_name].get_keys = get_keys
        _M[zone_name].get = get_kv_value

    elseif shm.type == types.ffi_shm.SHM_TYPE_QUEUE then
        -- NYI

    elseif shm.type == types.ffi_shm.SHM_TYPE_METRICS then
        _M[zone_name].get_keys = get_keys
        _M[zone_name].get = get_metric
    end
end)


function _M.init()
    if initialized then
        return
    end

    local rc = C.ngx_wa_ffi_shm_get_zones(get_zones_handler)
    if rc == FFI_DECLINED then
        ngx_log(ngx.INFO, "no shm zones found")
    end

    initialized = true
end


_M.init()


return _M
