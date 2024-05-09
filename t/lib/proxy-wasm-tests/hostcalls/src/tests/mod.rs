pub mod echo;
pub mod host;

use crate::{types::*, *};
use chrono::{DateTime, Utc};
use log::*;
use proxy_wasm::hostcalls::*;
use proxy_wasm::types::*;
use urlencoding::decode;

extern "C" {
    fn proxy_get_header_map_value(
        map_type: i32,
        key_data: *const u8,
        key_size: usize,
        return_value_data: *mut *mut u8,
        return_value_size: *mut usize,
    ) -> i32;
}

pub(crate) fn test_log_levels(_: &TestHttp) {
    trace!("proxy_log trace");
    info!("proxy_log info");
    warn!("proxy_log warn");
    error!("proxy_log error");
    log(LogLevel::Critical, "proxy_log critical").unwrap();
}

pub(crate) fn test_log_current_time(ctx: &TestHttp) {
    let now: DateTime<Utc> = ctx.get_current_time().into();
    info!("now: {}", now)
}

pub(crate) fn test_log_request_header(ctx: &TestHttp) {
    if let Some(header_name) = ctx.config.get("name") {
        let value = ctx.get_http_request_header(header_name.as_str());
        if value.is_some() {
            info!("request header \"{}: {}\"", header_name, value.unwrap());
        }
    }
}

pub(crate) fn test_log_request_headers(ctx: &TestHttp) {
    for (name, value) in ctx.get_http_request_headers() {
        info!("{}: {}", name, value)
    }
}

pub(crate) fn test_log_request_body(ctx: &TestHttp) {
    let body = ctx.get_http_request_body(0, 30);
    if let Some(bytes) = body {
        match String::from_utf8(bytes) {
            Ok(s) => info!("request body: {}", s),
            Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
        }
    }
}

pub(crate) fn test_log_response_body(ctx: &TestHttp) {
    let max_len = ctx
        .config
        .get("max_len")
        .map(|max| max.parse::<usize>().unwrap())
        .unwrap_or(usize::MAX);

    if let Some(bytes) = ctx.get_http_response_body(0, max_len) {
        match String::from_utf8(bytes) {
            Ok(s) => info!("response body chunk: {:?}", s),
            Err(e) => panic!("Invalid UTF-8 sequence: {}", e),
        }
    }
}

pub(crate) fn test_log_response_header(ctx: &TestHttp) {
    if let Some(header_name) = ctx.config.get("name") {
        let value = ctx.get_http_response_header(header_name.as_str());
        if value.is_some() {
            info!("resp header \"{}: {}\"", header_name, value.unwrap());
        }
    }
}

pub(crate) fn test_log_response_headers(ctx: &TestHttp) {
    for (name, value) in ctx.get_http_response_headers() {
        info!("resp {}: {}", name, value)
    }
}

pub(crate) fn test_log_request_path(ctx: &TestHttp) {
    let path = ctx
        .get_http_request_header(":path")
        .expect("failed to retrieve request path");

    info!("path: {}", path);
}

pub(crate) fn test_log_property(ctx: &(dyn TestContext + 'static)) {
    let name = ctx.get_config("name").expect("expected a name argument");

    match ctx.get_property(name.split('.').collect()) {
        Some(p) => match std::str::from_utf8(&p) {
            Ok(value) => info!("{}: {}", name, value),
            Err(_) => panic!("failed converting {} to UTF-8", name),
        },
        None => info!("property not found: {}", name),
    }
}

pub(crate) fn test_log_properties(ctx: &(dyn TestContext + 'static), phase: TestPhase) {
    let name = ctx.get_config("name").expect("expected a name argument");

    for property in name.split(',') {
        match ctx.get_property(property.split('.').collect()) {
            Some(p) => match std::str::from_utf8(&p) {
                Ok(value) => {
                    let decoded = decode(value).expect("UTF-8");
                    info!("{}: {} at {:?}", property, decoded, phase);
                }
                Err(_) => panic!("failed converting {} to UTF-8", property),
            },
            None => info!("property not found: {} at {:?}", property, phase),
        }
    }
}

pub(crate) fn test_log_metrics(ctx: &(dyn TestContext + 'static), phase: TestPhase) {
    for (n, id) in ctx.get_metrics_mapping() {
        if n.starts_with('h') { continue }
        let value = get_metric(*id).unwrap();
        info!("{}: {} at {:?}", n, value, phase)
    }
}

fn show_property(ctx: &(dyn TestContext + 'static), path: &[&str]) -> String {
    if let Some(p) = ctx.get_property(path.to_vec()) {
        if let Ok(value) = std::str::from_utf8(&p) {
            return format!("\"{value}\"");
        }
    }

    "unset".to_string()
}

pub(crate) fn test_set_property(ctx: &(dyn TestContext + 'static)) {
    let name = ctx.get_config("name").expect("expected a name argument");
    let path: Vec<&str> = name.split('.').collect();

    let show_old = ctx.get_config("show_old").map(|value| value.as_bytes());
    if show_old != Some(b"false") {
        info!("old: {}", show_property(ctx, &path));
    }

    let value = ctx.get_config("set").map(|value| value.as_bytes());

    // proxy-wasm-rust-sdk returns Ok(()) or panics
    ctx.set_property(path.clone(), value);

    info!("new: {}", show_property(ctx, &path));
}

pub(crate) fn test_send_status(ctx: &TestHttp, status: u32) {
    ctx.send_http_response(status, vec![], None);
}

pub(crate) fn test_send_headers(ctx: &TestHttp) {
    ctx.send_http_response(200, vec![("Powered-By", "proxy-wasm")], None)
}

pub(crate) fn test_send_body(ctx: &TestHttp) {
    //let path = ctx.get_http_request_header(":path").unwrap();
    ctx.send_http_response(
        200,
        vec![("Content-Length", "0")], // must be overriden to body.len() by host
        Some("Hello world".as_bytes()),
        //Some(format!("Hello world ({})", path).as_bytes()),
    )
}

pub(crate) fn test_send_twice(ctx: &TestHttp) {
    ctx.send_http_response(200, vec![], Some("Send once".as_bytes()));
    ctx.send_http_response(201, vec![], Some("Send twice".as_bytes()))
}

pub(crate) fn test_set_special_headers(ctx: &TestHttp) {
    ctx.send_http_response(
        200,
        vec![
            ("Server", "proxy-wasm"),
            ("Date", "Wed, 22 Oct 2020 07:28:00 GMT"),
            ("Content-Length", "3"), // No effect (overriden)
            ("Content-Encoding", "gzip"),
            ("Location", "/index.html"),
            ("Refresh", "5; url=http://www.w3.org/index.html"),
            ("Last-Modified", "Tue, 15 Nov 1994 12:45:26 GMT"),
            ("Content-Range", "bytes 21010-47021/47022"),
            ("Accept-Ranges", "bytes"),
            ("WWW-Authenticate", "Basic"),
            ("Expires", "Thu, 01 Dec 1994 16:00:00 GMT"),
            //("ETag", "737060cd8c284d8af7ad3082f209582d"),
            ("E-Tag", "377060cd8c284d8af7ad3082f20958d2"),
            ("Content-Type", "text/plain; charset=UTF-8"),
            ("Cache-Control", "no-cache"),
            ("Link", "</feed>; rel=\"alternate\""),
        ],
        None,
    )
}

pub(crate) fn test_set_headers_escaping(ctx: &TestHttp) {
    ctx.send_http_response(
        200,
        vec![
            ("Escape-Colon:", "value"),
            ("Escape-Parenthesis()", "value"),
            ("Escape-Quote\"", "value"),
            ("Escape-Comps<>", "value"),
            ("Escape-Equal=", "value"),
        ],
        None,
    )
}

pub(crate) fn test_set_request_headers(ctx: &TestHttp) {
    ctx.set_http_request_headers(vec![("Hello", "world"), ("Welcome", "wasm")]);
}

pub(crate) fn test_set_request_headers_special(ctx: &TestHttp) {
    ctx.set_http_request_headers(vec![
        (":path", "/updated"),
        ("Host", "somehost"),
        ("Connection", "closed"),
        ("User-Agent", "Gecko"),
        ("Content-Type", "text/none"),
        ("X-Forwarded-For", "128.168.0.1"),
    ]);
}

pub(crate) fn test_set_request_headers_invalid(ctx: &TestHttp) {
    ctx.set_http_request_headers(vec![(":scheme", "https")]);
}

trait HeaderStr {
    fn split_header<'a>(&'a self) -> Option<(&'a str, &'a str)>;
}

impl HeaderStr for str {
    fn split_header<'a>(&'a self) -> Option<(&'a str, &'a str)> {
        if &self[0..1] == ":" {
            self[1..]
                .find(':')
                .map(|n| (&self[0..n + 1], &self[n + 2..]))
        } else {
            self.split_once(':')
        }
    }
}

pub(crate) fn test_set_request_header(ctx: &TestHttp) {
    if let Some(name) = ctx.config.get("name") {
        let value = ctx.config.get("value").expect("missing value");
        ctx.set_http_request_header(name, Some(value));
    } else if let Some(config) = ctx.config.get("value") {
        let (name, value) = config.split_header().unwrap();
        if value.is_empty() {
            ctx.set_http_request_header(name, None);
        } else {
            ctx.set_http_request_header(name, Some(value));
        }
    }
}

pub(crate) fn test_add_request_header(ctx: &TestHttp) {
    if let Some(config) = ctx.config.get("value") {
        let (name, value) = config.split_header().unwrap();
        ctx.add_http_request_header(name, value);
    } else if let Some(header) = ctx.get_http_request_header("pwm-add-req-header") {
        let (name, value) = header.split_once('=').unwrap();
        ctx.add_http_request_header(name, value);
    }
}

pub(crate) fn test_add_response_header(ctx: &TestHttp) {
    if let Some(header) = ctx.config.get("value") {
        let (name, value) = header.split_header().unwrap();
        ctx.add_http_response_header(name, value);
    }
}

pub(crate) fn test_set_response_header(ctx: &TestHttp) {
    if let Some(header) = ctx.config.get("value") {
        let (name, value) = header.split_header().unwrap();
        if value.is_empty() {
            ctx.set_http_response_header(name, None);
        } else {
            ctx.set_http_response_header(name, Some(value));
        }
    }
}

pub(crate) fn test_set_response_headers(ctx: &TestHttp) {
    if let Some(headers_str) = ctx.config.get("value") {
        let headers = headers_str
            .split('+')
            .filter_map(|s| s.split_header())
            .collect();

        ctx.set_http_response_headers(headers);
    } else {
        ctx.set_http_response_headers(vec![("Hello", "world"), ("Welcome", "wasm")]);
    }
}

pub(crate) fn test_set_request_body(ctx: &TestHttp) {
    let body = if let Some(value) = ctx.config.get("value") {
        value.to_string()
    } else {
        "Hello world".into()
    };

    let offset = ctx
        .config
        .get("offset")
        .map_or(0, |v| v.parse::<usize>().unwrap());

    let len = ctx
        .config
        .get("max")
        .map_or(body.len(), |v| v.parse::<usize>().unwrap());

    ctx.set_http_request_body(offset, len, body.as_bytes());
}

pub(crate) fn test_set_response_body(ctx: &mut TestHttp) {
    let key = "set_response_body".to_string();
    if ctx.config.get(&key).is_some() {
        return;
    }

    let mut body = if let Some(value) = ctx.config.get("value") {
        value.to_string()
    } else {
        "Hello world".into()
    };

    if !body.is_empty() {
        body.push('\n');
    }

    let offset = ctx
        .config
        .get("offset")
        .map_or(0, |v| v.parse::<usize>().unwrap());

    let len = ctx
        .config
        .get("max")
        .map_or(body.len(), |v| v.parse::<usize>().unwrap());

    ctx.set_http_response_body(offset, len, body.as_bytes());

    ctx.config.insert(key.clone(), key);
}

pub(crate) fn test_get_shared_data(ctx: &TestHttp) {
    let hcas = ctx
        .config
        .get("header_cas")
        .map(|x| x.as_str())
        .unwrap_or("cas");

    let hdata = ctx
        .config
        .get("header_data")
        .map(|x| x.as_str())
        .unwrap_or("data");

    let hexists = ctx
        .config
        .get("header_exists")
        .map(|x| x.as_str())
        .unwrap_or("exists");

    let (data, cas) = ctx.get_shared_data(ctx.config.get("key").unwrap());
    let cas_value = cas
        .map(|x| format!("{x}"))
        .unwrap_or_else(|| "0".to_string());

    ctx.add_http_response_header(hcas, cas_value.as_str());
    ctx.add_http_response_header(hexists, if data.is_some() { "1" } else { "0" });
    ctx.add_http_response_header(
        hdata,
        std::str::from_utf8(data.as_deref().unwrap_or_default()).unwrap(),
    );
}

pub(crate) fn test_set_shared_data(ctx: &TestHttp) {
    let cas = ctx.config.get("cas").map(|x| x.parse::<u32>().unwrap());

    let hok = ctx
        .config
        .get("header_ok")
        .map(|x| x.as_str())
        .unwrap_or("set-ok");

    let ok = ctx
        .set_shared_data(
            ctx.config.get("key").unwrap(),
            ctx.config.get("value").map(|x| x.as_bytes()),
            cas,
        )
        .is_ok();

    ctx.add_http_response_header(hok, if ok { "1" } else { "0" });
}

pub(crate) fn test_set_shared_data_by_len(ctx: &mut TestHttp) {
    let len = ctx
        .config
        .get("len")
        .map_or(0, |v| v.parse::<usize>().unwrap());

    ctx.config.insert("value".to_string(), "x".repeat(len));

    test_set_shared_data(ctx);
}

fn should_run_on_current_worker(ctx: &(dyn TestContext + 'static)) -> bool {
    match ctx.get_config("on_worker") {
        Some(on_worker) => {
            let w_id_bytes = ctx
                .get_property(vec![("worker_id")])
                .expect("could not get worker_id");
            let w_id = std::str::from_utf8(&w_id_bytes).expect("bad worker_id value");

            on_worker == w_id
        }
        None => true,
    }
}

pub(crate) fn test_define_metrics(ctx: &mut (dyn TestContext + 'static)) {
    if !should_run_on_current_worker(ctx) {
        return;
    }

    let name_len = ctx.get_config("metrics_name_len").map_or(0, |x| {
        x.parse::<usize>().expect("bad metrics_name_len value")
    });

    let metrics_config = ctx
        .get_config("metrics")
        .map(|x| x.to_string())
        .expect("missing metrics parameter");

    for metric in metrics_config.split(",") {
        let metric_char = metric.chars().nth(0).unwrap();
        let metric_type = match metric_char {
            'c' => MetricType::Counter,
            'g' => MetricType::Gauge,
            'h' => MetricType::Histogram,
            _ => panic!("unexpected metric type"),
        };
        let n = metric[1..].parse::<u64>().expect("bad metrics value");

        for i in 1..(n + 1) {
            let mut name = format!("{}{}", metric_char, i);

            if name_len > 0 {
                name = format!("{}{}", name, "x".repeat(name_len - name.chars().count()));
            }

            let m_id = define_metric(metric_type, &name).expect("cannot define new metric");

            info!("defined metric {} as {:?}", &name, m_id);

            ctx.save_metric_mapping(name.as_str(), m_id);
        }
    }
}

pub(crate) fn test_increment_counters(ctx: &(dyn TestContext + 'static), phase: TestPhase, skip_others: Option<bool>) {
    if !should_run_on_current_worker(ctx) {
        return;
    }

    let n_increments = ctx
        .get_config("n_increments")
        .map_or(1, |x| x.parse::<u64>().expect("bad n_increments value"));

    for (n, id) in ctx.get_metrics_mapping() {
        if skip_others.unwrap_or(true) && !n.starts_with('c') { continue }

        for _ in 0..n_increments {
            increment_metric(*id, 1).unwrap();
        }

        info!("incremented {} at {:?}", n, phase);
    }

    test_log_metrics(ctx, phase);
}

pub(crate) fn test_toggle_gauges(ctx: &(dyn TestContext + 'static), phase: TestPhase, skip_others: Option<bool>) {
    if !should_run_on_current_worker(ctx) {
        return;
    }

    for (n, id) in ctx.get_metrics_mapping() {
        if skip_others.unwrap_or(true) && !n.starts_with('g') { continue }

        let new_value = match get_metric(*id).unwrap() {
            0 => 1,
            _ => 0,
        };

        record_metric(*id, new_value).unwrap();
        info!("toggled {} at {:?}", n, phase);
    }

    test_log_metrics(ctx, phase);
}

pub(crate) fn test_record_metric(ctx: &(dyn TestContext + 'static), phase: TestPhase) {
    if !should_run_on_current_worker(ctx) {
        return;
    }

    let value = ctx
        .get_config("value")
        .map_or(1, |x| x.parse::<u64>().expect("bad value"));

    for (n, id) in ctx.get_metrics_mapping() {
        if n.starts_with('c') { continue }
        record_metric(*id, value).unwrap();
        info!("record {} on {} at {:?}", value, n, phase);
    }

    test_log_metrics(ctx, phase);
}

pub(crate) fn test_get_metrics(ctx: &TestHttp) {
    for (n, id) in ctx.get_metrics_mapping() {
        if n.starts_with('h') { continue }

        let name = n.replace("_", "-");
        let value = get_metric(*id).unwrap().to_string();
        ctx.add_http_response_header(name.as_str(), value.as_str());
    }
}

pub(crate) fn test_shared_queue_enqueue(ctx: &TestHttp) {
    let queue_id: u32 = ctx
        .config
        .get("queue_id")
        .map(|x| x.parse().unwrap())
        .unwrap_or_else(|| ctx.register_shared_queue(ctx.config.get("queue").unwrap()));

    let data = ctx.config.get("value").cloned().unwrap_or_else(|| {
        let len: u32 = ctx.config.get("length").unwrap().parse().unwrap();
        String::from_utf8(vec![b'a'; len as usize]).unwrap()
    });

    let hstatus = ctx
        .config
        .get("header_status")
        .map(|x| x.as_str())
        .unwrap_or("status-enqueue");

    let status: u32 = ctx
        .enqueue_shared_queue(queue_id, Some(data.as_bytes()))
        .map(|_| 0)
        .unwrap_or_else(|x| x as u32);

    ctx.add_http_response_header(hstatus, format!("{status}").as_str());
}

pub(crate) fn test_shared_queue_dequeue(ctx: &TestHttp) {
    let queue_id: u32 = ctx
        .config
        .get("queue_id")
        .map(|x| x.parse().unwrap())
        .unwrap_or_else(|| ctx.register_shared_queue(ctx.config.get("queue").unwrap()));

    let hstatus = ctx
        .config
        .get("header_status")
        .map(|x| x.as_str())
        .unwrap_or("status-dequeue");

    let hdata = ctx
        .config
        .get("header_data")
        .map(|x| x.as_str())
        .unwrap_or("data");

    let hexists = ctx
        .config
        .get("header_exists")
        .map(|x| x.as_str())
        .unwrap_or("exists");

    let res = ctx.dequeue_shared_queue(queue_id);
    let status = format!(
        "{}",
        match &res {
            Ok(_) => 0,
            Err(e) => *e as u32,
        }
    );

    let data = res
        .as_ref()
        .map(|x| x.as_ref().map(|x| std::str::from_utf8(x).unwrap()))
        .unwrap_or_default();
    ctx.add_http_response_header(hstatus, status.as_str());

    if !hdata.is_empty() {
        ctx.add_http_response_header(hdata, data.unwrap_or_default());
    }

    ctx.add_http_response_header(hexists, if data.is_some() { "1" } else { "0" });
}

pub(crate) fn test_proxy_get_header_map_value_oob_key(_ctx: &TestHttp) {
    let mut return_data: *mut u8 = std::ptr::null_mut();
    let mut return_size: usize = 0;
    unsafe {
        proxy_get_header_map_value(
            0,
            0xdeadbeef as *const u8,
            42,
            &mut return_data,
            &mut return_size,
        );
    }
}

pub(crate) fn test_proxy_get_header_map_value_oob_return_data(_ctx: &TestHttp) {
    let mut return_size: usize = 0;
    let key = b"Connection";
    unsafe {
        proxy_get_header_map_value(
            0,
            key.as_ptr(),
            key.len(),
            0xf0000000 as *mut *mut u8,
            &mut return_size,
        );
    }
}

pub(crate) fn test_proxy_get_header_map_value_misaligned_return_data(_ctx: &TestHttp) {
    let mut return_data: *mut u8 = std::ptr::null_mut();
    let mut return_size: usize = 0;
    let key = b"Connection";
    unsafe {
        proxy_get_header_map_value(
            0,
            key.as_ptr(),
            key.len(),
            ((&mut return_data as *mut *mut u8) as usize + 1) as *mut *mut u8,
            &mut return_size,
        );
    }
}
