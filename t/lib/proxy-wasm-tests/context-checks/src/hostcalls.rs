use crate::*;
use log::*;
use proxy_wasm::types::*;
use std::ptr::null_mut;

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_log(level: u32, message_data: *const u8, message_size: usize) -> Status;
}

pub fn log_something(_ctx: &TestContext) {
    let msg = "proxy_log msg";
    unsafe {
        let status = proxy_log(2, msg.as_ptr(), msg.len());
        info!("proxy_log status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_set_tick_period_milliseconds(period: u32) -> Status;
}

pub fn set_tick_period(_ctx: &TestContext) {
    unsafe {
        let status = proxy_set_tick_period_milliseconds(10000);
        info!("set_tick_period status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_add_header_map_value(
        map_type: MapType,
        key_data: *const u8,
        key_size: usize,
        value_data: *const u8,
        value_size: usize,
    ) -> Status;
}

pub fn add_request_header(_ctx: &TestContext, arg: &str) {
    let vec: Vec<(&str, &str)> = arg
        .split(',')
        .map(|kv| kv.split_once('='))
        .flatten()
        .collect();

    for (name, value) in &vec {
        unsafe {
            let status = proxy_add_header_map_value(
                MapType::HttpRequestHeaders,
                name.as_ptr(),
                name.len(),
                value.as_ptr(),
                value.len(),
            );

            info!("add_request_header status: {}", status as u32);
        }
    }
}

pub fn add_response_header(_ctx: &TestContext, arg: &str) {
    let vec: Vec<(&str, &str)> = arg
        .split(',')
        .map(|kv| kv.split_once('='))
        .flatten()
        .collect();

    for (name, value) in &vec {
        unsafe {
            let status = proxy_add_header_map_value(
                MapType::HttpResponseHeaders,
                name.as_ptr(),
                name.len(),
                value.as_ptr(),
                value.len(),
            );

            info!("add_response_header status: {}", status as u32);
        }
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_replace_header_map_value(
        map_type: MapType,
        key_data: *const u8,
        key_size: usize,
        value_data: *const u8,
        value_size: usize,
    ) -> Status;
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_remove_header_map_value(
        map_type: MapType,
        key_data: *const u8,
        key_size: usize,
    ) -> Status;
}

pub fn set_request_header(_ctx: &TestContext, arg: &str) {
    let vec: Vec<(&str, &str)> = arg
        .split(',')
        .map(|kv| kv.split_once('='))
        .flatten()
        .collect();
    let mut status;

    for (name, value) in &vec {
        unsafe {
            if !value.is_empty() {
                status = proxy_replace_header_map_value(
                    MapType::HttpRequestHeaders,
                    name.as_ptr(),
                    name.len(),
                    value.as_ptr(),
                    value.len(),
                );
            } else {
                status = proxy_remove_header_map_value(
                    MapType::HttpRequestHeaders,
                    name.as_ptr(),
                    name.len(),
                );
            }

            info!("set_request_header status: {}", status as u32);
        }
    }
}

pub fn set_response_header(_ctx: &TestContext, arg: &str) {
    let vec: Vec<(&str, &str)> = arg
        .split(',')
        .map(|kv| kv.split_once('='))
        .flatten()
        .collect();
    let mut status;

    for (name, value) in &vec {
        unsafe {
            if !value.is_empty() {
                status = proxy_replace_header_map_value(
                    MapType::HttpResponseHeaders,
                    name.as_ptr(),
                    name.len(),
                    value.as_ptr(),
                    value.len(),
                );
            } else {
                status = proxy_remove_header_map_value(
                    MapType::HttpResponseHeaders,
                    name.as_ptr(),
                    name.len(),
                );
            }

            info!("set_response_header status: {}", status as u32);
        }
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_set_header_map_pairs(
        map_type: MapType,
        map_data: *const u8,
        map_size: usize,
    ) -> Status;
}

// internal function from proxy-wasm-rust-sdk
fn serialize_map(map: Vec<(&str, &str)>) -> Bytes {
    let mut size: usize = 4;
    for (name, value) in &map {
        size += name.len() + value.len() + 10;
    }
    let mut bytes: Bytes = Vec::with_capacity(size);
    bytes.extend_from_slice(&map.len().to_le_bytes());
    for (name, value) in &map {
        bytes.extend_from_slice(&name.len().to_le_bytes());
        bytes.extend_from_slice(&value.len().to_le_bytes());
    }
    for (name, value) in &map {
        bytes.extend_from_slice(name.as_bytes());
        bytes.push(0);
        bytes.extend_from_slice(value.as_bytes());
        bytes.push(0);
    }
    bytes
}

pub fn set_request_headers(_ctx: &TestContext, arg: &str) {
    let vec: Vec<(&str, &str)> = arg
        .split(',')
        .map(|kv| kv.split_once('='))
        .flatten()
        .collect();
    let serialized = serialize_map(vec);

    unsafe {
        let status = proxy_set_header_map_pairs(
            MapType::HttpRequestHeaders,
            serialized.as_ptr(),
            serialized.len(),
        );

        info!("set_request_headers status: {}", status as u32);
    }
}

pub fn set_response_headers(_ctx: &TestContext, arg: &str) {
    let vec: Vec<(&str, &str)> = arg
        .split(',')
        .map(|kv| kv.split_once('='))
        .flatten()
        .collect();
    let serialized = serialize_map(vec);

    unsafe {
        let status = proxy_set_header_map_pairs(
            MapType::HttpResponseHeaders,
            serialized.as_ptr(),
            serialized.len(),
        );

        info!("set_response_headers status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_set_buffer_bytes(
        buffer_type: u32,
        start: usize,
        size: usize,
        buffer_data: *const u8,
        buffer_size: usize,
    ) -> Status;
}

pub fn set_request_body_buffer(_ctx: &TestContext) {
    let v = String::default();

    unsafe {
        let status = proxy_set_buffer_bytes(
            BufferType::HttpRequestBody as u32,
            0,
            0,
            v.as_ptr(),
            v.len(),
        );

        info!("set_request_body_buffer status: {}", status as u32);
    }
}

pub fn set_response_body_buffer(_ctx: &TestContext) {
    let v = String::default();

    unsafe {
        let status = proxy_set_buffer_bytes(
            BufferType::HttpResponseBody as u32,
            0,
            0,
            v.as_ptr(),
            v.len(),
        );

        info!("set_response_body_buffer status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_get_buffer_bytes(
        buffer_type: u32,
        start: usize,
        max_size: usize,
        return_buffer_data: *mut *mut u8,
        return_buffer_size: *mut usize,
    ) -> Status;
}

pub fn get_request_body_buffer(_ctx: &TestContext) {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;

    unsafe {
        let status = proxy_get_buffer_bytes(
            BufferType::HttpRequestBody as u32,
            0,
            0,
            &mut return_data,
            &mut return_size,
        );

        info!("get_request_body_buffer status: {}", status as u32);
    }
}

pub fn get_response_body_buffer(_ctx: &TestContext) {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;

    unsafe {
        let status = proxy_get_buffer_bytes(
            BufferType::HttpResponseBody as u32,
            0,
            0,
            &mut return_data,
            &mut return_size,
        );

        info!("get_response_body_buffer status: {}", status as u32);
    }
}

pub fn get_dispatch_response_body_buffer(_ctx: &TestContext) {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;

    unsafe {
        let status = proxy_get_buffer_bytes(
            BufferType::HttpCallResponseBody as u32,
            0,
            0,
            &mut return_data,
            &mut return_size,
        );

        info!(
            "get_dispatch_response_body_buffer status: {}",
            status as u32
        );
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_http_call(
        upstream_data: *const u8,
        upstream_size: usize,
        headers_data: *const u8,
        headers_size: usize,
        body_data: *const u8,
        body_size: usize,
        trailers_data: *const u8,
        trailers_size: usize,
        timeout: u32,
        return_token: *mut u32,
    ) -> Status;
}

pub fn dispatch_http_call(_ctx: &TestContext) {
    let upstream = "";
    let serialized_headers = "";
    let serialized_trailers = "";
    let body = "";
    let mut return_token: u32 = 0;

    unsafe {
        let status = proxy_http_call(
            upstream.as_ptr(),
            0,
            serialized_headers.as_ptr(),
            0,
            body.as_ptr(),
            0,
            serialized_trailers.as_ptr(),
            0,
            0,
            &mut return_token,
        );

        info!("dispatch_http_call status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_define_metric(
        metric_type: MetricType,
        name_data: *const u8,
        name_size: usize,
        return_id: *mut u32,
    ) -> Status;
}

pub fn define_metric(_ctx: &TestContext) {
    let name = "a_counter";
    let metric_type = MetricType::Counter;
    let mut return_id: u32 = 0;

    unsafe {
        let status = proxy_define_metric(metric_type, name.as_ptr(), name.len(), &mut return_id);

        info!("define_metric status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_get_metric(metric_id: u32, return_value: *mut u64) -> Status;
}

pub fn get_metric(_ctx: &TestContext) {
    let name = "a_counter";
    let metric_type = MetricType::Counter;
    let mut metric_id: u32 = 0;
    let mut value: u64 = 0;

    unsafe {
        proxy_define_metric(metric_type, name.as_ptr(), name.len(), &mut metric_id);
        let status = proxy_get_metric(metric_id, &mut value);
        info!("get_metric status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_record_metric(metric_id: u32, value: u64) -> Status;
}

pub fn record_metric(_ctx: &TestContext) {
    let name = "a_gauge";
    let metric_type = MetricType::Gauge;
    let mut metric_id: u32 = 0;

    unsafe {
        proxy_define_metric(metric_type, name.as_ptr(), name.len(), &mut metric_id);
        let status = proxy_record_metric(metric_id, 1);
        info!("record_metric status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_increment_metric(metric_id: u32, offset: i64) -> Status;
}

pub fn increment_metric(_ctx: &TestContext) {
    let name = "a_counter";
    let metric_type = MetricType::Counter;
    let mut metric_id: u32 = 0;

    unsafe {
        proxy_define_metric(metric_type, name.as_ptr(), name.len(), &mut metric_id);
        let status = proxy_increment_metric(metric_id, 1);
        info!("increment_metric status: {}", status as u32);
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_call_foreign_function(
        function_name_data: *const u8,
        function_name_size: usize,
        arguments_data: *const u8,
        arguments_size: usize,
        results_data: *mut *mut u8,
        results_size: *mut usize,
    ) -> Status;
}

pub fn call_resolve_lua(_ctx: &TestContext, name: &str) {
    let f_name = "resolve_lua";

    let mut ret_data: *mut u8 = null_mut();
    let mut ret_size: usize = 0;

    unsafe {
        let status = proxy_call_foreign_function(
            f_name.as_ptr(),
            f_name.len(),
            name.as_ptr(),
            name.len(),
            &mut ret_data,
            &mut ret_size,
        );

        info!("resolve_lua status: {}", status as u32);
    }
}
