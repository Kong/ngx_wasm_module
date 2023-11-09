mod hostcalls;

use crate::hostcalls::*;
use log::*;
use proxy_wasm::{traits::*, types::*};
use std::collections::HashMap;
use std::time::Duration;

pub struct TestContext {
    config: HashMap<String, String>,
}

impl TestContext {
    fn get_config(&self, name: &str) -> Option<&str> {
        self.config.get(name).map(|s| s.as_str())
    }

    fn check_host_function(&self, name: &str) {
        match name {
            "proxy_log" => log_something(self),
            "set_tick_period" => set_tick_period(self),
            "set_request_body_buffer" => set_request_body_buffer(self),
            "set_response_body_buffer" => set_response_body_buffer(self),
            "get_request_body_buffer" => get_request_body_buffer(self),
            "get_response_body_buffer" => get_response_body_buffer(self),
            "get_dispatch_response_body_buffer" => get_dispatch_response_body_buffer(self),
            "dispatch_http_call" => dispatch_http_call(self),
            _ => (),
        }
    }
}

struct TestRoot {
    tester: TestContext,
}

struct TestHttp {
    tester: TestContext,
}

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestRoot {
            tester: TestContext {
                config: HashMap::new(),
            }
        })
    });
}}

impl HttpContext for TestHttp {
    fn on_http_request_headers(&mut self, _: usize, _: bool) -> Action {
        if let Some(name) = self.tester.get_config("on_request_headers") {
            self.tester.check_host_function(name);
        } else if self
            .tester
            .get_config("on_http_dispatch_response")
            .is_some()
        {
            let _ = self.dispatch_http_call(
                self.tester.get_config("host").unwrap_or(""),
                vec![(":path", "/dispatch"), (":method", "GET")],
                None,
                vec![],
                Duration::from_secs(0),
            );

            return Action::Pause;
        }

        Action::Continue
    }

    fn on_http_request_body(&mut self, _: usize, _: bool) -> Action {
        if let Some(name) = self.tester.get_config("on_request_body") {
            self.tester.check_host_function(name);
        }

        Action::Continue
    }

    fn on_http_response_headers(&mut self, _: usize, _: bool) -> Action {
        if let Some(name) = self.tester.get_config("on_response_headers") {
            self.tester.check_host_function(name);
        }

        Action::Continue
    }

    fn on_http_response_body(&mut self, _: usize, _: bool) -> Action {
        if let Some(name) = self.tester.get_config("on_response_body") {
            self.tester.check_host_function(name);
        }

        Action::Continue
    }

    fn on_log(&mut self) {
        if let Some(name) = self.tester.get_config("on_log") {
            self.tester.check_host_function(name);
        }
    }
}

impl Context for TestHttp {
    fn on_http_call_response(
        &mut self,
        token_id: u32,
        nheaders: usize,
        body_size: usize,
        ntrailers: usize,
    ) {
        let status = self.get_http_call_response_header(":status");

        info!(
            "on_http_call_response (id: {}, status: {}, headers: {}, body_bytes: {}, trailers: {})",
            token_id,
            status.unwrap_or("".to_string()),
            nheaders,
            body_size,
            ntrailers
        );

        if let Some(name) = self.tester.get_config("on_http_dispatch_response") {
            self.tester.check_host_function(name);
        }
    }
}

impl RootContext for TestRoot {
    fn on_vm_start(&mut self, _: usize) -> bool {
        info!("on_vm_start");

        if let Some(config) = self.get_vm_configuration() {
            match std::str::from_utf8(&config) {
                Ok(text) => {
                    info!("vm config: {}", text);
                    self.tester.check_host_function(text);
                }
                _ => info!("cannot parse vm config"),
            }
        }

        true
    }

    fn on_configure(&mut self, _: usize) -> bool {
        info!("on_configure");

        if let Some(config_bytes) = self.get_plugin_configuration() {
            let config_str = String::from_utf8(config_bytes).unwrap();
            self.tester.config = config_str
                .split_whitespace()
                .filter_map(|s| s.split_once('='))
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect();
        } else {
            self.tester.config = HashMap::new();
        }

        if self.tester.get_config("on_tick").is_some() {
            self.set_tick_period(Duration::from_millis(100));
        }

        if let Some(name) = self.tester.get_config("on_configure") {
            self.tester.check_host_function(name);
        }

        true
    }

    fn on_tick(&mut self) {
        info!("on_tick",);

        if let Some(name) = self.tester.get_config("on_tick") {
            self.tester.check_host_function(name);
        }
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, _: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(TestHttp {
            tester: TestContext {
                config: self.tester.config.clone(),
            },
        }))
    }
}

impl Context for TestRoot {
    fn on_http_call_response(
        &mut self,
        token_id: u32,
        nheaders: usize,
        body_size: usize,
        ntrailers: usize,
    ) {
        let status = self.get_http_call_response_header(":status");

        info!(
            "on_root_http_call_response (id: {}, status: {}, headers: {}, body_bytes: {}, trailers: {})",
            token_id, status.unwrap_or("".to_string()), nheaders, body_size, ntrailers
        );

        if let Some(name) = self.tester.get_config("on_http_dispatch_response") {
            self.tester.check_host_function(name);
        }
    }
}
