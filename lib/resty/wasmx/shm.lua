-- vim:set ts=4 sw=4 sts=4 et:

local ffi = require "ffi"
local wasmx = require "resty.wasmx"


local C = ffi.C
local error = error
local type = type
local tonumber = tonumber
local min = math.min
local new_tab = table.new
local insert = table.insert
local str_fmt = string.format
local ffi_cast = ffi.cast
local ffi_fill = ffi.fill
local ffi_new = ffi.new
local ffi_str = ffi.string
local ngx_sleep = ngx.sleep
local ngx_log = ngx.log
local DEBUG = ngx.DEBUG
local FFI_OK = wasmx.FFI_OK
local FFI_ERROR = wasmx.FFI_ERROR
local FFI_DONE = wasmx.FFI_DONE
local FFI_BUSY = wasmx.FFI_BUSY
local FFI_ABORT = wasmx.FFI_ABORT
local FFI_DECLINED = wasmx.FFI_DECLINED
local assert_debug = wasmx.assert_debug


ffi.cdef [[
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

    typedef enum {
        NGX_WA_HISTOGRAM_LOG2,
        NGX_WA_HISTOGRAM_CUSTOM,
    } ngx_wa_histogram_type_e;

    typedef struct {
        ngx_uint_t                   value;
        ngx_msec_t                   last_update;
    } ngx_wa_metrics_gauge_t;

    typedef struct {
        uint32_t                     upper_bound;
        uint32_t                     count;
    } ngx_wa_metrics_bin_t;

    typedef struct {
        ngx_wa_histogram_type_e      h_type;
        uint8_t                      n_bins;
        uint64_t                     sum;
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


    ngx_int_t ngx_wa_ffi_shm_setup_zones(ngx_wa_ffi_shm_setup_zones_handler handler);
    ngx_int_t ngx_wa_ffi_shm_iterate_keys(ngx_wa_shm_t *shm,
                                          ngx_uint_t page_size,
                                          ngx_uint_t *clast_index,
                                          ngx_uint_t *cur_idx,
                                          ngx_str_t **keys);

    ngx_uint_t ngx_wa_ffi_shm_kv_nelts(ngx_wa_shm_t *shm);
    ngx_int_t ngx_wa_ffi_shm_kv_get(ngx_wa_shm_t *shm,
                                    ngx_str_t *k,
                                    ngx_str_t **v,
                                    uint32_t *cas);
    ngx_int_t ngx_wa_ffi_shm_kv_set(ngx_wa_shm_t *shm,
                                    ngx_str_t *k,
                                    ngx_str_t *v,
                                    uint32_t cas,
                                    unsigned *written);

    ngx_int_t ngx_wa_ffi_shm_metric_define(ngx_str_t *name,
                                           ngx_wa_metric_type_e type,
                                           uint32_t *bins,
                                           uint16_t n_bins,
                                           uint32_t *metric_id);
    ngx_int_t ngx_wa_ffi_shm_metric_increment(uint32_t metric_id,
                                              ngx_uint_t value);
    ngx_int_t ngx_wa_ffi_shm_metric_record(uint32_t metric_id,
                                           ngx_uint_t value);
    ngx_int_t ngx_wa_ffi_shm_metric_get(uint32_t metric_id,
                                        ngx_str_t *name,
                                        u_char *mbuf, size_t mbs,
                                        u_char *hbuf, size_t hbs);

    void ngx_wa_ffi_shm_lock(ngx_wa_shm_t *shm);
    void ngx_wa_ffi_shm_unlock(ngx_wa_shm_t *shm);

    ngx_int_t ngx_wa_ffi_shm_metrics_one_slot_size();
    ngx_int_t ngx_wa_ffi_shm_metrics_histogram_max_size();
    ngx_int_t ngx_wa_ffi_shm_metrics_histogram_max_bins();
]]


local WASM_SHM_KEY = {}
local DEFAULT_KEYS_PAGE_SIZE = 500
local HISTOGRAM_MAX_BINS = C.ngx_wa_ffi_shm_metrics_histogram_max_bins()


local _M = setmetatable({}, {
    __index = function(_, k)
        error("resty.wasmx.shm: no \"" .. k .. "\" shm configured", 2)
    end,
})


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

local _mbs = C.ngx_wa_ffi_shm_metrics_one_slot_size()
local _hbs = C.ngx_wa_ffi_shm_metrics_histogram_max_size()
local _mbuf = ffi_new("u_char[?]", _mbs)
local _hbuf = ffi_new("u_char[?]", _hbs)
local _kbuf = ffi_new("ngx_str_t *[?]", DEFAULT_KEYS_PAGE_SIZE)


local function shm_lock(zone)
    C.ngx_wa_ffi_shm_lock(zone[WASM_SHM_KEY])
end


local function shm_unlock(zone)
    C.ngx_wa_ffi_shm_unlock(zone[WASM_SHM_KEY])
end


local function key_iterator(ctx)
    if ctx.i == tonumber(ctx.ccur_index[0]) then
        ngx_sleep(0) -- TODO: support non-yielding phases

        ctx.i = 0
        ctx.ccur_index[0] = 0

        local rc = C.ngx_wa_ffi_shm_iterate_keys(ctx.shm, ctx.page_size,
                                                 ctx.clast_index, ctx.ccur_index,
                                                 ctx.ckeys)

        if rc == FFI_ABORT then
            -- users must manage locking themselves (e.g. break condition in the for loop)
            local zone_name = ffi_str(ctx.shm.name.data, ctx.shm.name.len)
            local err = "attempt to call %s:iterate_keys() but the " ..
                        "shm zone is not locked; invoke :lock() before " ..
                        "and :unlock() after."

            error(str_fmt(err, zone_name), 2)
        end

        if rc == FFI_DONE then
            return
        end

        assert_debug(rc == FFI_OK)

        ngx_log(DEBUG, "iterate_keys fetched a new page")
    end

    local ckey = ctx.ckeys[ctx.i]
    local key = ffi_str(ctx.ckeys[ctx.i].data, ctx.ckeys[ctx.i].len)

    ctx.i = ctx.i + 1

    return key
end


local function shm_iterate_keys(zone, opts)
    if opts ~= nil then
        if type(opts) ~= "table" then
            error("opts must be a table", 2)
        end

        if opts.page_size ~= nil then
            if type(opts.page_size) ~= "number" then
                error("opts.page_size must be a number", 2)
            end

            if opts.page_size < 1 then
                error("opts.page_size must be > 0", 2)
            end
        end
    end

    local ctx = {
        zone = zone,
        shm = zone[WASM_SHM_KEY],
        page_size = opts and opts.page_size
                    and opts.page_size
                    or DEFAULT_KEYS_PAGE_SIZE,
        i = 0,
        clast_index = ffi_new("ngx_uint_t[1]"),
        ccur_index = ffi_new("ngx_uint_t[1]"),
        ckeys = page_size and ffi_new("ngx_str_t *[?]", page_size) or _kbuf,
    }

    return key_iterator, ctx
end


local function shm_get_keys(zone, max_count)
    if max_count == nil then
        max_count = DEFAULT_KEYS_PAGE_SIZE

    elseif type(max_count) ~= "number" then
        error("max_count must be a number", 2)

    elseif max_count < 0 then
        error("max_count must be >= 0", 2)
    end

    shm_lock(zone)

    local shm = zone[WASM_SHM_KEY]
    local nkeys = tonumber(C.ngx_wa_ffi_shm_kv_nelts(shm))
    if nkeys == 0 then
        shm_unlock(zone)
        return {}
    end

    if max_count > 0 then
        nkeys = min(nkeys, max_count)
    end

    local ctotal = ffi_new("ngx_uint_t[1]")
    local ckeys = ffi_new("ngx_str_t *[?]", nkeys)

    C.ngx_wa_ffi_shm_iterate_keys(shm, nkeys, nil, ctotal, ckeys)

    shm_unlock(zone)

    local total = tonumber(ctotal[0])
    local keys = new_tab(0, total)
    assert(total == nkeys)

    for i = 1, total do
        keys[i] = ffi_str(ckeys[i - 1].data, ckeys[i - 1].len)
    end

    return keys
end


local function shm_kv_get(zone, key)
    if type(key) ~= "string" then
        error("key must be a string", 2)
    end

    local shm = zone[WASM_SHM_KEY]
    local cname = ffi_new("ngx_str_t", { data = key, len = #key })
    local cvalue = ffi_new("ngx_str_t *[1]")
    local ccas = ffi_new("uint32_t[1]")

    local rc = C.ngx_wa_ffi_shm_kv_get(shm, cname, cvalue, ccas)
    if rc == FFI_DECLINED then
        return nil
    end

    assert_debug(rc == FFI_OK)

    return ffi_str(cvalue[0].data, cvalue[0].len), tonumber(ccas[0])
end


local function shm_kv_set(zone, key, value, cas)
    if type(key) ~= "string" then
        error("key must be a string", 2)
    end

    if type(value) ~= "string" then
        error("value must be a string", 2)
    end

    if cas == nil then
        cas = 0

    elseif type(cas) ~= "number" then
        error("cas must be a number", 2)
    end

    local shm = zone[WASM_SHM_KEY]
    local cname = ffi_new("ngx_str_t", { data = key, len = #key })
    local cvalue = ffi_new("ngx_str_t", { data = value, len = #value })
    local written = ffi_new("unsigned[1]")

    local rc = C.ngx_wa_ffi_shm_kv_set(shm, cname, cvalue, cas, written)
    if rc == FFI_ERROR then
        return nil, "no memory"
    end

    if rc == FFI_ABORT then
        return nil, "locked"
    end

    assert_debug(rc == FFI_OK)

    return tonumber(written[0])
end


local function metrics_define(zone, name, metric_type, opts)
    if type(name) ~= "string" or name == "" then
        error("name must be a non-empty string", 2)
    end

    if not _metric_type_set[metric_type] then
        local err = "metric_type must be one of" ..
                    " resty.wasmx.shm.metrics.COUNTER," ..
                    " resty.wasmx.shm.metrics.GAUGE, or" ..
                    " resty.wasmx.shm.metrics.HISTOGRAM"
        error(err, 2)
    end

    local cbins
    local n_bins = 0

    if opts ~= nil then
        if type(opts) ~= "table" then
            error("opts must be a table", 2)
        end

        if metric_type == _types.ffi_metric.HISTOGRAM
           and opts.bins ~= nil
        then
            if type(opts.bins) ~= "table" then
                error("opts.bins must be a table", 2)
            end

            if #opts.bins >= HISTOGRAM_MAX_BINS then
                local err = "opts.bins cannot have more than %d numbers"
                error(str_fmt(err, HISTOGRAM_MAX_BINS - 1), 2)
            end

            local previous = 0

            for _, n in ipairs(opts.bins) do
                if type(n) ~= "number"
                   or n < 0 or n % 1 > 0 or n <= previous
                then
                    error("opts.bins must be an ascending list of " ..
                          "positive integers", 2)
                end

                previous = n
            end

            n_bins = #opts.bins
            cbins = ffi_new("uint32_t[?]", n_bins, opts.bins)
        end
    end

    name = "lua:" .. name

    local cname = ffi_new("ngx_str_t", { data = name, len = #name })
    local m_id = ffi_new("uint32_t[1]")

    local rc = C.ngx_wa_ffi_shm_metric_define(cname, metric_type,
                                              cbins, n_bins, m_id)
    if rc == FFI_ERROR then
        return nil, "no memory"
    end

    if rc == FFI_BUSY then
        return nil, "name too long"
    end

    -- FFI_ABORT: unreachable
    assert_debug(rc == FFI_OK)

    return tonumber(m_id[0])
end


local function metrics_increment(zone, metric_id, value)
    if type(metric_id) ~= "number" then
        error("metric_id must be a number", 2)
    end

    if value ~= nil and (type(value) ~= "number" or value < 1) then
        error("value must be > 0", 2)
    end

    value = value and value or 1

    local rc = C.ngx_wa_ffi_shm_metric_increment(metric_id, value)
    if rc == FFI_DECLINED then
        return nil, "metric not found"
    end

    assert_debug(rc == FFI_OK)

    return true
end


local function metrics_record(zone, metric_id, value)
    if type(metric_id) ~= "number" then
        error("metric_id must be a number", 2)
    end

    if type(value) ~= "number" then
        error("value must be a number", 2)
    end

    local rc = C.ngx_wa_ffi_shm_metric_record(metric_id, value)
    if rc == FFI_DECLINED then
        return nil, "metric not found"
    end

    assert_debug(rc == FFI_OK)

    return true
end


local function parse_cmetric(cmetric)
    if cmetric.metric_type == _types.ffi_metric.COUNTER then
        return {
            type = "counter",
            value = tonumber(cmetric.slots[0].counter),
        }
    end

    if cmetric.metric_type == _types.ffi_metric.GAUGE then
        return {
            type = "gauge",
            value = tonumber(cmetric.slots[0].gauge.value),
        }
    end

    if cmetric.metric_type == _types.ffi_metric.HISTOGRAM then
        local hbuf = cmetric.slots[0].histogram
        local ch = ffi_cast("ngx_wa_metrics_histogram_t *", hbuf)
        local h = { type = "histogram", value = {}, sum = tonumber(ch.sum) }

        for i = 0, (ch.n_bins - 1) do
            local cb = ch.bins[i]
            if cb.upper_bound == 0 then
                break
            end

            insert(h.value, {
                ub = cb.upper_bound,
                count = cb.count,
            })
        end

        return h
    end

    assert(false, "unreachable")
end


local function metrics_get_by_id(zone, metric_id)
    if type(metric_id) ~= "number" then
        error("metric_id must be a number", 2)
    end

    ffi_fill(_mbuf, _mbs)
    ffi_fill(_hbuf, _hbs)

    local rc = C.ngx_wa_ffi_shm_metric_get(metric_id, nil,
                                           _mbuf, _mbs,
                                           _hbuf, _hbs)
    if rc == FFI_DECLINED then
        return nil
    end

    assert_debug(rc == FFI_OK)

    return parse_cmetric(ffi_cast("ngx_wa_metric_t *", _mbuf))
end


---
-- ngx_wasm_module internally prefixes metric names with colon-separated
-- metadata indicating where they have been defined, e.g. "pw:a_filter:*",
-- "lua:*", or "wa:".
--
-- metrics_get_by_name assumes it is retrieving a Lua-defined metric
-- and will by default prefix the given name with `lua:`
--
-- This behavior can be disabled by passing `opts.prefix` as false.
local function metrics_get_by_name(zone, name, opts)
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

    ffi_fill(_mbuf, _mbs)
    ffi_fill(_hbuf, _hbs)

    name = (opts and opts.prefix == false) and name or "lua:" .. name

    local cname = ffi_new("ngx_str_t", { data = name, len = #name })

    local rc = C.ngx_wa_ffi_shm_metric_get(0, cname, _mbuf, _mbs, _hbuf, _hbs)
    if rc == FFI_DECLINED then
        return nil
    end

    assert_debug(rc == FFI_OK)

    return parse_cmetric(ffi_cast("ngx_wa_metric_t *", _mbuf))
end


local _setup_zones_handler = ffi_cast("ngx_wa_ffi_shm_setup_zones_handler",
function(shm)
    local zone_name = ffi_str(shm.name.data, shm.name.len)
    _M[zone_name] = {
        [WASM_SHM_KEY] = shm,
        lock = shm_lock,
        unlock = shm_unlock,
    }

    if shm.type == _types.ffi_shm.SHM_TYPE_KV then
        _M[zone_name].iterate_keys = shm_iterate_keys
        _M[zone_name].get_keys = shm_get_keys
        _M[zone_name].get = shm_kv_get
        _M[zone_name].set = shm_kv_set

    elseif shm.type == _types.ffi_shm.SHM_TYPE_QUEUE then
        -- NYI

    elseif shm.type == _types.ffi_shm.SHM_TYPE_METRICS then
        _M[zone_name].get_keys = shm_get_keys
        _M[zone_name].iterate_keys = shm_iterate_keys

        _M[zone_name].define = metrics_define
        _M[zone_name].increment = metrics_increment
        _M[zone_name].record = metrics_record
        _M[zone_name].get = metrics_get_by_id
        _M[zone_name].get_by_name = metrics_get_by_name

        _M[zone_name].COUNTER = _types.ffi_metric.COUNTER
        _M[zone_name].GAUGE = _types.ffi_metric.GAUGE
        _M[zone_name].HISTOGRAM = _types.ffi_metric.HISTOGRAM
    end
end)


if C.ngx_wa_ffi_shm_setup_zones(_setup_zones_handler) == FFI_ABORT then
    ngx_log(DEBUG, "no shm zones found for resty.wasmx.shm interface")
end


return _M
