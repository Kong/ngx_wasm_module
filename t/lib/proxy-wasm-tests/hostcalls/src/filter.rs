mod dispatch;
mod echo;
mod test_cases;

use crate::{dispatch::*, echo::*, test_cases::*};
use http::StatusCode;
use log::*;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::collections::HashMap;
use std::time::Duration;
use url::Url;

#[derive(Debug, Eq, PartialEq, enum_utils::FromStr)]
#[enumeration(rename_all = "snake_case")]
enum TestPhase {
    RequestHeaders,
    RequestBody,
    ResponseHeaders,
    ResponseBody,
    Log,
}

trait TestContext {
    fn get_config(&self, name: &str) -> Option<&String>;
}

impl Context for dyn TestContext {}

struct TestRoot {
    config: HashMap<String, String>,
}

impl Context for TestRoot {}

impl TestContext for TestRoot {
    fn get_config(&self, name: &str) -> Option<&String> {
        self.config.get(name)
    }
}

impl RootContext for TestRoot {
    fn on_configure(&mut self, _: usize) -> bool {
        if let Some(config_bytes) = self.get_plugin_configuration() {
            let config_str = String::from_utf8(config_bytes).unwrap();
            self.config = config_str
                .split_whitespace()
                .filter_map(|s| s.split_once('='))
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect();
        } else {
            self.config = HashMap::new();
        }

        if let Some(period) = self.config.get("tick_period") {
            self.set_tick_period(Duration::from_millis(
                period.parse().expect("bad tick_period"),
            ));
        }

        true
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn on_tick(&mut self) {
        if let Some(period) = self.config.get("tick_period") {
            info!("[hostcalls] on_tick, {}", period);

            match self.config.get("test").unwrap().as_str() {
                "/t/log/property" => test_log_property(self),
                _ => (),
            }
        }
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        info!("create context id #{}", context_id);

        Some(Box::new(TestHttpHostcalls {
            config: self.config.clone(),
            on_phase: self
                .config
                .get("on")
                .map_or(TestPhase::RequestHeaders, |s| {
                    s.parse()
                        .unwrap_or_else(|_| panic!("unknown phase: {:?}", s))
                }),
            request_body_size: None,
        }))
    }
}

struct TestHttpHostcalls {
    on_phase: TestPhase,
    config: HashMap<String, String>,
    request_body_size: Option<usize>,
}

impl TestHttpHostcalls {
    fn send_plain_response(&mut self, status: StatusCode, body: Option<String>) {
        if let Some(b) = body {
            self.send_http_response(status.as_u16() as u32, vec![], Some(b.as_bytes()))
        } else {
            self.send_http_response(status.as_u16() as u32, vec![], None)
        }
    }

    fn _send_not_found(&mut self) {
        self.send_plain_response(StatusCode::NOT_FOUND, None)
    }

    fn exec_tests(&mut self, cur_phase: TestPhase) -> Action {
        if cur_phase != self.on_phase {
            return Action::Continue;
        }

        info!("[hostcalls] testing in \"{:?}\"", self.on_phase);

        let path = self.get_http_request_header(":path").unwrap();
        if path.starts_with("/t/echo/header/") {
            let res = Url::options()
                .base_url(Some(&Url::parse("http://localhost").unwrap()))
                .parse(&path);

            if let Err(e) = res {
                error!("{}", e);
            }

            /* /t/echo/header/{header_name: String} */

            let url = res.unwrap();
            let header_name = url
                .path()
                .split('/')
                .collect::<Vec<&str>>()
                .split_off(4)
                .pop()
                .unwrap();

            echo_header(self, header_name);
            return Action::Continue;
        }

        match self.config.get("test").unwrap_or(&path).as_str() {
            /* log */
            "/t/log/request_body" => test_log_request_body(self),
            "/t/log/levels" => test_log_levels(self),
            "/t/log/request_header" => test_log_request_header(self),
            "/t/log/request_headers" => test_log_request_headers(self),
            "/t/log/request_path" => test_log_request_path(self),
            "/t/log/response_header" => test_log_response_header(self),
            "/t/log/response_headers" => test_log_response_headers(self),
            "/t/log/response_body" => test_log_response_body(self),
            "/t/log/current_time" => test_log_current_time(self),
            "/t/log/property" => test_log_property(self),

            /* send_local_response */
            "/t/send_local_response/status/204" => test_send_status(self, 204),
            "/t/send_local_response/status/300" => test_send_status(self, 300),
            "/t/send_local_response/status/1000" => test_send_status(self, 1000),
            "/t/send_local_response/headers" => test_send_headers(self),
            "/t/send_local_response/body" => test_send_body(self),
            "/t/send_local_response/twice" => test_send_twice(self),
            "/t/send_local_response/set_special_headers" => test_set_special_headers(self),
            "/t/send_local_response/set_headers_escaping" => test_set_headers_escaping(self),

            /* set/add request/response headers */
            "/t/set_request_headers" => test_set_request_headers(self),
            "/t/set_request_headers/special" => test_set_request_headers_special(self),
            "/t/set_request_headers/invalid" => test_set_request_headers_invalid(self),
            "/t/set_response_headers" => test_set_response_headers(self),
            "/t/set_request_header" => test_set_http_request_header(self),
            "/t/set_response_header" => test_set_response_header(self),
            "/t/add_request_header" => test_add_http_request_header(self),
            "/t/add_response_header" => test_add_http_response_header(self),

            /* set/add request/response body */
            "/t/set_request_body" => test_set_http_request_body(self),
            "/t/set_response_body" => test_set_response_body(self),

            /* echo request */
            "/t/echo/headers" => echo_headers(self),
            "/t/echo/headers/all" => echo_all_headers(self),
            "/t/echo/body" => echo_body(self, self.request_body_size),

            /* http dispatch */
            "/t/dispatch_http_call" => {
                dispatch_call(self);
                return Action::Pause;
            }

            /* errors */
            "/t/error/get_response_body" => {
                let _body = self.get_http_response_body(usize::MAX, usize::MAX);
            }
            "/t/trap" => {
                panic!("trap");
            }

            _ => (),
        }

        Action::Continue
    }
}

impl Context for TestHttpHostcalls {
    fn on_http_call_response(
        &mut self,
        token_id: u32,
        num_headers: usize,
        body_size: usize,
        _num_trailers: usize,
    ) {
        info!(
            "[hostcalls] in http_call_response (token_id: {}, n_headers: {}, body_len: {})",
            token_id, num_headers, body_size
        );

        if self.config.get("trap").is_some() {
            panic!("trap!");
        }

        if let Some(bytes) = self.get_http_call_response_body(0, 1000) {
            let body = String::from_utf8(bytes).unwrap();
            let response = body.trim().to_string();
            info!("[hostcalls] http_call_response: {:?}", response);

            match self
                .config
                .get("echo_response")
                .unwrap_or(&String::new())
                .as_str()
            {
                "off" | "false" | "F" => {}
                _ => self.send_plain_response(StatusCode::OK, Some(response)),
            }
        }

        self.resume_http_request()
    }
}

impl TestContext for TestHttpHostcalls {
    fn get_config(&self, name: &str) -> Option<&String> {
        self.config.get(name)
    }
}

impl HttpContext for TestHttpHostcalls {
    fn on_http_request_headers(&mut self, nheaders: usize, _eof: bool) -> Action {
        info!("[hostcalls] on_request_headers, {} headers", nheaders);
        self.exec_tests(TestPhase::RequestHeaders)
    }

    fn on_http_request_body(&mut self, size: usize, end_of_stream: bool) -> Action {
        self.request_body_size = Some(size);
        info!(
            "[hostcalls] on_request_body, {} bytes, end_of_stream: {}",
            size, end_of_stream
        );
        self.exec_tests(TestPhase::RequestBody)
    }

    fn on_http_response_headers(&mut self, nheaders: usize, _eof: bool) -> Action {
        info!("[hostcalls] on_response_headers, {} headers", nheaders);
        self.exec_tests(TestPhase::ResponseHeaders)
    }

    fn on_http_response_body(&mut self, len: usize, end_of_stream: bool) -> Action {
        info!(
            "[hostcalls] on_response_body, {} bytes, end_of_stream {}",
            len, end_of_stream
        );
        self.exec_tests(TestPhase::ResponseBody)
    }

    fn on_log(&mut self) {
        info!("[hostcalls] on_log");
        self.exec_tests(TestPhase::Log);
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestRoot {
            config: HashMap::new(),
        })
    });
}
