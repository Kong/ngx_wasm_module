use crate::*;
use std::ptr::{null, null_mut};

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

pub(crate) fn test_set_buffer_bad_type() {
    let v = String::default();

    unsafe {
        match proxy_set_buffer_bytes(100, 0, 0, v.as_ptr(), v.len()) {
            Status::Ok => (),
            status => panic!("unexpected status: {}", status as u32),
        }
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

pub(crate) fn test_get_buffer_bad_type() {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;

    unsafe {
        match proxy_get_buffer_bytes(100, 0, 0, &mut return_data, &mut return_size) {
            Status::Ok => (),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_set_header_map_pairs(map_type: u32, map_data: *const u8, map_size: usize) -> Status;
}

pub(crate) fn test_set_map_bad_type() {
    unsafe {
        match proxy_set_header_map_pairs(100, null(), 0) {
            Status::Ok => (),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_get_header_map_pairs(
        map_type: u32,
        return_map_data: *mut *mut u8,
        return_map_size: *mut usize,
    ) -> Status;
}

pub(crate) fn test_get_map_bad_type() {
    let mut return_data: *mut u8 = null_mut();
    let mut return_size: usize = 0;

    unsafe {
        match proxy_get_header_map_pairs(100, &mut return_data, &mut return_size) {
            Status::Ok => (),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_log(level: u32, message_data: *const u8, message_size: usize) -> Status;
}

pub(crate) fn test_bad_log_level() {
    unsafe {
        match proxy_log(100, String::default().as_ptr(), String::default().len()) {
            Status::Ok => (),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}

#[allow(improper_ctypes)]
extern "C" {
    fn proxy_close_stream(stream_type: StreamType) -> Status;
}

pub fn test_nyi_host_func() {
    unsafe {
        match proxy_close_stream(StreamType::Downstream) {
            Status::Ok => (),
            status => panic!("unexpected status: {}", status as u32),
        }
    }
}
