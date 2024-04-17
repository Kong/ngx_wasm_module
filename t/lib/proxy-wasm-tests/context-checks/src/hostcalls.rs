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
            serialized.len());

        info!("set_response_headers status: {}", status as u32);
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
