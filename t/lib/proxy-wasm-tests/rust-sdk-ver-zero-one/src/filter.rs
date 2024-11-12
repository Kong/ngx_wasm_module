#![allow(clippy::single_match)]

use http::StatusCode;
use log::*;
use proxy_wasm::{traits::*, types::*};
use std::collections::HashMap;
use std::time::Duration;

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestRoot {
            config: HashMap::new(),
        })
    });
}

struct TestRoot {
    config: HashMap<String, String>,
}

impl Context for TestRoot {}
impl RootContext for TestRoot {
    fn on_configure(&mut self, _: usize) -> bool {
        if let Some(config_bytes) = self.get_configuration() {
            let config_str = String::from_utf8(config_bytes).unwrap();
            self.config = config_str
                .split_whitespace()
                .filter_map(|s| s.split_once('='))
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect();
        } else {
            self.config = HashMap::new();
        }

        true
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, _: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(TestHttpHostcalls {
            config: self.config.clone(),
        }))
    }
}

struct TestHttpHostcalls {
    config: HashMap<String, String>,
}

impl Context for TestHttpHostcalls {
    fn on_http_call_response(
        &mut self,
        _token_id: u32,
        _nheaders: usize,
        body_size: usize,
        _ntrailers: usize,
    ) {
        let bytes = self.get_http_call_response_body(0, body_size);
        let op = self
            .config
            .get("on_http_call_response")
            .map_or("", |v| v.as_str());

        match op {
            "echo_response_body" => {
                if let Some(response) = bytes {
                    let body = String::from_utf8_lossy(&response);
                    self.send_http_response(
                        StatusCode::OK.as_u16() as u32,
                        vec![],
                        Some(body.trim().as_bytes()),
                    );
                }
            }
            _ => {}
        }

        self.resume_http_request()
    }
}

impl HttpContext for TestHttpHostcalls {
    fn on_http_request_headers(&mut self, _: usize) -> Action {
        if let Some(test) = self.config.get("test") {
            match test.as_str() {
                "dispatch" => {
                    match self.dispatch_http_call(
                        self.config.get("host").map(|v| v.as_str()).unwrap_or(""),
                        vec![
                            (":method", "GET"),
                            (
                                ":path",
                                self.config.get("path").map(|v| v.as_str()).unwrap_or("/"),
                            ),
                            (
                                ":authority",
                                self.config.get("host").map(|v| v.as_str()).unwrap_or(""),
                            ),
                        ],
                        self.config.get("body").map(|v| v.as_bytes()),
                        vec![],
                        Duration::from_secs(3),
                    ) {
                        Ok(_) => {}
                        Err(status) => match status {
                            Status::BadArgument => error!("dispatch_http_call: bad argument"),
                            Status::InternalFailure => {
                                error!("dispatch_http_call: internal failure")
                            }
                            status => panic!(
                                "dispatch_http_call: unexpected status \"{}\"",
                                status as u32
                            ),
                        },
                    }

                    return Action::Pause;
                }
                "proxy_get_header_map_value" => {
                    let ct = self.get_http_request_header("Connection").unwrap();
                    self.send_http_response(
                        StatusCode::OK.as_u16() as u32,
                        vec![],
                        Some(ct.as_bytes()),
                    );
                }
                "proxy_get_empty_map_value" => {
                    let ct = self.get_http_request_header("None");
                    self.send_http_response(
                        StatusCode::OK.as_u16() as u32,
                        vec![],
                        Some(ct.unwrap_or_default().as_bytes()),
                    );
                }
                _ => todo!(),
            };
        }

        Action::Continue
    }
}
