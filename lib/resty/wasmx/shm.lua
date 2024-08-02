-- vim:set ts=4 sw=4 sts=4 et:

local ffi = require "ffi"
local wasmx = require "resty.wasmx"


local C = ffi.C
local error = error
local type = type
local table_new = table.new
local str_fmt = string.format
local ffi_cast = ffi.cast
local ffi_fill = ffi.fill
local ffi_new = ffi.new
local ffi_str = ffi.string
local ngx_log = ngx.log
local ngx_debug = ngx.DEBUG
local ngx_warn = ngx.WARN
local ngx_sleep = ngx.sleep
local FFI_ABORT = wasmx.FFI_ABORT
local FFI_DECLINED = wasmx.FFI_DECLINED
local FFI_ERROR = wasmx.FFI_ERROR
local FFI_OK = wasmx.FFI_OK


ffi.cdef [[
    typedef unsigned char            u_char;
    typedef uintptr_t                ngx_uint_t;
    typedef ngx_uint_t               ngx_msec_t;
    typedef struct ngx_log_s         ngx_log_t;
    typedef struct ngx_slab_pool_s   ngx_slab_pool_t;

    typedef enum {
        NGX_WA_SHM_TYPE_KV,
        NGX_WA_SHM_TYPE_QUEUE,
        NGX_WA_SHM_TYPE_METRICS,
    } ngx_wa_shm_type_e;

    typedef enum {
        NGX_WA_SHM_EVICTION_LRU,
        NGX_WA_SHM_EVICTION_SLRU,
        NGX_WA_SHM_EVICTION_NONE,
    } ngx_wa_shm_eviction_e;

    typedef struct {
        ngx_wa_shm_type_e            type;
        ngx_wa_shm_eviction_e        eviction;
        ngx_str_t                    name;
        ngx_log_t                   *log;
        ngx_slab_pool_t             *shpool;
        void                        *data;
    } ngx_wa_shm_t;

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


    typedef void (*ngx_wa_ffi_shm_setup_zones_handler)(ngx_wa_shm_t *shm);


    int ngx_wa_ffi_shm_setup_zones(ngx_wa_ffi_shm_setup_zones_handler handler);
    int ngx_wa_ffi_shm_iterate_keys(ngx_wa_shm_t *shm,
                                    ngx_uint_t page,
                                    ngx_uint_t page_size,
                                    ngx_str_t **keys,
                                    ngx_uint_t *total);
    void ngx_wa_ffi_shm_lock(ngx_wa_shm_t *shm);
    void ngx_wa_ffi_shm_unlock(ngx_wa_shm_t *shm);

    int ngx_wa_ffi_shm_get_kv_value(ngx_wa_shm_t *shm,
                                    ngx_str_t *k,
                                    ngx_str_t **v,
                                    uint32_t *cas);
    int ngx_wa_ffi_shm_set_kv_value(ngx_wa_shm_t *shm,
                                    ngx_str_t *k,
                                    ngx_str_t *v,
                                    uint32_t cas,
                                    int *written);

    int ngx_wa_ffi_shm_define_metric(ngx_str_t *name,
                                     ngx_wa_metric_type_e type,
                                     uint32_t *metric_id);
    int ngx_wa_ffi_shm_record_metric(uint32_t metric_id, int value);
    int ngx_wa_ffi_shm_increment_metric(uint32_t metric_id, int value);
    int ngx_wa_ffi_shm_get_metric(uint32_t metric_id,
                                  ngx_str_t *name,
                                  u_char *mbuf, size_t mbs,
                                  u_char *hbuf, size_t hbs);

    int ngx_wa_ffi_shm_one_slot_metric_size();
    int ngx_wa_ffi_shm_max_histogram_size();
]]


local WASM_SHM_KEY = {}
local DEFAULT_KEY_BATCH_SIZE = 500


local _M = {}


local _initialized = false
local _types = {
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
local _metric_type_set = {
    [_types.ffi_metric.COUNTER] = true,
    [_types.ffi_metric.GAUGE] = true,
    [_types.ffi_metric.HISTOGRAM] = true,
}
local _kbuf = ffi_new("ngx_str_t *[?]", DEFAULT_KEY_BATCH_SIZE)
local _mbs = C.ngx_wa_ffi_shm_one_slot_metric_size()
local _mbuf = ffi_new("u_char [?]", _mbs)
local _hbs = C.ngx_wa_ffi_shm_max_histogram_size()
local _hbuf = ffi_new("u_char [?]", _hbs)


local function lock_shm(zone)
    C.ngx_wa_ffi_shm_lock(zone[WASM_SHM_KEY])
end


local function unlock_shm(zone)
    C.ngx_wa_ffi_shm_unlock(zone[WASM_SHM_KEY])
end


local function key_iterator(ctx)
    if ctx.i < ctx.page_total then
        local ckey = ctx.ckeys[ctx.i]
        ctx.i = ctx.i + 1

        return ffi_str(ckey.data, ckey.len)

    else
        ctx.cpage_total[0] = 0

        local rc = C.ngx_wa_ffi_shm_iterate_keys(ctx.shm,
                                                 ctx.page, ctx.page_size,
                                                 ctx.ckeys, ctx.cpage_total)
        if rc == FFI_ABORT then
            local zone_name = ffi_str(ctx.shm.name.data, ctx.shm.name.len)
            local err = "attempt to iterate over the keys of an unlocked shm zone."
                .. " please call resty.wasmx.shm.%s:lock() before calling"
                .. " iterate_keys() and resty.wasmx.shm.%s:unlock() after"

            error(str_fmt(err, zone_name, zone_name), 2)
        end

        if rc == FFI_DECLINED then
            return
        end

        ngx_sleep(0)

        ctx.page_total = tonumber(ctx.cpage_total[0])
        ctx.page = ctx.page + ctx.page_total
        ctx.i = 1

        return ffi_str(ctx.ckeys[0].data, ctx.ckeys[0].len)
    end
end


local function iterate_keys(zone, page_size)
    if page_size ~= nil and type(page_size) ~= "number" then
        error("page_size must be a number", 2)
    end

    local ctx = {
        shm = zone[WASM_SHM_KEY],
        i = 0,
        page = 0,
        page_size = page_size and page_size or DEFAULT_KEY_BATCH_SIZE,
        page_total = 0,
        cpage_total = ffi_new("ngx_uint_t [1]"),
        ckeys = page_size and ffi_new("ngx_str_t *[?]", page_size) or _kbuf
    }

    return key_iterator, ctx
end


local function get_keys(zone, max)
    if max ~= nil and type(max) ~= "number" then
        error("max must be number", 2)
    end

    local shm = zone[WASM_SHM_KEY]
    local nkeys
    local keys
    local start = 0
    local ctotal = ffi_new("ngx_uint_t [1]")

    lock_shm(zone)

    if max == 0 then
        local rc = C.ngx_wa_ffi_shm_iterate_keys(shm, start, 0, nil, ctotal)
        nkeys = tonumber(ctotal[0])

        if rc == FFI_DECLINED then
            unlock_shm(zone)
            return {}
        end

    else
        nkeys = max and max or DEFAULT_KEY_BATCH_SIZE
    end

    local ckeys = ffi_new("ngx_str_t *[?]", nkeys)

    C.ngx_wa_ffi_shm_iterate_keys(shm, start, nkeys, ckeys, ctotal)

    unlock_shm(zone)

    local total = tonumber(ctotal[0])
    local keys = table_new(0, total)

    for i = 1, total do
        local ckey = ckeys[i - 1]
        keys[i] = ffi_str(ckey.data, ckey.len)
    end

    return keys
end


local function get_kv_value(zone, key)
    if type(key) ~= "string" then
        error("key must be string", 2)
    end

    local shm = zone[WASM_SHM_KEY]
    local cname = ffi_new("ngx_str_t", { data = key, len = #key })
    local cvalue = ffi_new("ngx_str_t *[1]")
    local ccas = ffi_new("uint32_t [1]")

    local rc = C.ngx_wa_ffi_shm_get_kv_value(shm, cname, cvalue, ccas)
    if rc == FFI_DECLINED then
        return nil, nil, "key not found"
    end

    return ffi_str(cvalue[0].data, cvalue[0].len), tonumber(ccas[0])
end


local function set_kv_value(zone, key, value, cas)
    if type(key) ~= "string" then
        error("key must be a string", 2)
    end

    if type(value) ~= "string" then
        error("value must be a string", 2)
    end

    if type(cas) ~= "number" then
        error("cas must be a number", 2)
    end

    local shm = zone[WASM_SHM_KEY]
    local cname = ffi_new("ngx_str_t", { data = key, len = #key })
    local cvalue = ffi_new("ngx_str_t", { data = value, len = #value })
    local written = ffi_new("uint32_t [1]")

    local rc = C.ngx_wa_ffi_shm_set_kv_value(shm, cname, cvalue, cas, written)
    if rc == FFI_ERR then
        return nil, "failed setting kv value, no memory"
    end

    return tonumber(written[0])
end


local function parse_cmetric(cmetric)
    if cmetric.metric_type == _types.ffi_metric.COUNTER then
        return { type = "counter", value = tonumber(cmetric.slots[0].counter) }

    elseif cmetric.metric_type == _types.ffi_metric.GAUGE then
        return { type = "gauge", value = tonumber(cmetric.slots[0].gauge.value) }

    elseif cmetric.metric_type == _types.ffi_metric.HISTOGRAM then
        local hbuf = cmetric.slots[0].histogram
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


local function get_metric(zone, metric_id)
    if type(metric_id) ~= "number" then
        error("metric_id must be a number", 2)
    end

    ffi_fill(_mbuf, _mbs)
    ffi_fill(_hbuf, _hbs)

    local rc = C.ngx_wa_ffi_shm_get_metric(metric_id, nil,
                                           _mbuf, _mbs, _hbuf, _hbs)
    if rc == FFI_DECLINED then
        return nil, "metric not found"
    end

    return parse_cmetric(ffi_cast("ngx_wa_metric_t *", _mbuf))
end


---
-- ngx_wasm_module internally prefixes metric names according to where they
-- have been defined, e.g. pw.filter.metric, lua.metric or wa.metric.
--
-- get_metric_by_name assumes that it's retrieving a lua land metric and
-- will by default prefix name with `lua.`
--
-- This behavior can be disabled by passing `opts.prefix` as false.
---
local function get_metric_by_name(zone, name, opts)
    if type(name) ~= "string" or name == "" then
        error("name must be a non-empty string", 2)
    end

    if opts ~= nil then
        if type(opts) ~= "table" then
            error("opts must be a table", 2)
        end

        if opts.prefix ~= nil and type(opts.prefix) ~= "boolean" then
            error("opts.prefix must be a boolean", 2)
        end
    end

    name = (opts and opts.prefix == false) and name or "lua." .. name
    local cname = ffi_new("ngx_str_t", { data = name, len = #name })

    ffi_fill(_mbuf, _mbs)
    ffi_fill(_hbuf, _hbs)

    local rc = C.ngx_wa_ffi_shm_get_metric(0, cname, _mbuf, _mbs, _hbuf, _hbs)
    if rc == FFI_DECLINED then
        return nil, "metric not found"
    end

    return parse_cmetric(ffi_cast("ngx_wa_metric_t *", _mbuf))
end


local function define_metric(zone, name, metric_type)
    if type(name) ~= "string" or name == "" then
        error("name must be a non-empty string", 2)
    end

    if _metric_type_set[metric_type] == nil then
        local err = "metric_type must be either"
            .. " resty.wasmx.shm.metrics.COUNTER,"
            .. " resty.wasmx.shm.metrics.GAUGE, or"
            .. " resty.wasmx.shm.metrics.HISTOGRAM"

        error(err, 2)
    end

    local cname = ffi_new("ngx_str_t", { data = name, len = #name })
    local m_id = ffi_new("uint32_t [1]")

    local rc = C.ngx_wa_ffi_shm_define_metric(cname, metric_type, m_id)
    if rc == FFI_ABORT then
        return nil, "failed defining metric, name too long"
    end

    if rc == FFI_ERROR then
        return nil, "failed defining metric, no memory"
    end

    return tonumber(m_id[0])
end


local function record_metric(zone, metric_id, value)
    if type(metric_id) ~= "number" then
        error("metric_id must be a number", 2)
    end

    if type(value) ~= "number" then
        error("value must be a number", 2)
    end

    local rc = C.ngx_wa_ffi_shm_record_metric(metric_id, value)
    if rc == FFI_DECLINED then
        return nil, "metric not found"
    end

    return true
end


local function increment_metric(zone, metric_id, value)
    if type(metric_id) ~= "number" then
        error("metric_id must be a number", 2)
    end

    if value ~= nil and (type(value) ~= "number" or value < 1) then
        error("value must be a number greater than zero", 2)
    end

    value = value and value or 1

    local rc = C.ngx_wa_ffi_shm_increment_metric(metric_id, value)
    if rc == FFI_DECLINED then
        return nil, "metric not found"
    end

    return true
end


local _setup_zones_handler = ffi_cast("ngx_wa_ffi_shm_setup_zones_handler",
function(shm)
    local zone_name = ffi_str(shm.name.data, shm.name.len)
    _M[zone_name] = {
        [WASM_SHM_KEY] = shm,
        lock = lock_shm,
        unlock = unlock_shm
    }

    if shm.type == _types.ffi_shm.SHM_TYPE_KV then
        _M[zone_name].get_keys = get_keys
        _M[zone_name].iterate_keys = iterate_keys
        _M[zone_name].get = get_kv_value
        _M[zone_name].set = set_kv_value

    elseif shm.type == _types.ffi_shm.SHM_TYPE_QUEUE then
        -- NYI

    elseif shm.type == _types.ffi_shm.SHM_TYPE_METRICS then
        _M[zone_name].get_keys = get_keys
        _M[zone_name].iterate_keys = iterate_keys
        _M[zone_name].get = get_metric
        _M[zone_name].get_by_name = get_metric_by_name
        _M[zone_name].define = define_metric
        _M[zone_name].increment = increment_metric
        _M[zone_name].record = record_metric
        _M[zone_name].COUNTER = _types.ffi_metric.COUNTER
        _M[zone_name].GAUGE = _types.ffi_metric.GAUGE
        _M[zone_name].HISTOGRAM = _types.ffi_metric.HISTOGRAM
    end
end)


function _M.init()
    if _initialized then
        return
    end

    local rc = C.ngx_wa_ffi_shm_setup_zones(_setup_zones_handler)
    if rc == FFI_ABORT then
        ngx_log(ngx.DEBUG, "no shm zones found for resty.wasmx interface")
    end

    _initialized = true
end


_M.init()


return _M
