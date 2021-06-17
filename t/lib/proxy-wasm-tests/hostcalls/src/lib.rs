mod echo;
mod test_cases;

use crate::{echo::*, test_cases::*};
use http::{Method, StatusCode};
use log::*;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::collections::HashMap;

#[derive(Debug, PartialEq, enum_utils::FromStr)]
#[enumeration(rename_all = "snake_case")]
enum TestPhase {
    HttpRequestHeaders,
    HttpRequestBody,
    HttpResponseHeaders,
    HttpResponseBody,
    Log,
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

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(TestHttpHostcalls {
            context_id,
            config: self.config.clone(),
            on_phase: self
                .config
                .get("on_phase")
                .map_or(TestPhase::HttpRequestHeaders, |s| {
                    s.parse()
                        .unwrap_or_else(|_| panic!("unknown phase: {:?}", s))
                }),
            request_body_size: None,
            response_body_size: None,
        }))
    }
}

struct TestHttpHostcalls {
    context_id: u32,
    on_phase: TestPhase,
    config: HashMap<String, String>,
    request_body_size: Option<usize>,
    response_body_size: Option<usize>,
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

    fn exec_tests(&mut self, cur_phase: TestPhase) {
        if cur_phase != self.on_phase {
            return;
        }

        info!("#{} entering \"{:?}\"", self.context_id, self.on_phase);

        let test_case;
        {
            let path = self.get_http_request_header(":path").unwrap();

            if let Some(c) = self.config.get("test_case") {
                test_case = c.to_string();
                debug!(
                    "#{} overriding test case from filter config: \"{}\"",
                    self.context_id, test_case
                );
            } else if let Some(h) = self.get_http_request_header("pwm-test-case") {
                // Subrequests currently retrieve their own location path (r->uri)
                // with :path, which means on_phases test cases would see the test
                // case's name as "/t/on_request_headers" instead of "/t/log/levels"
                // if we were to rely entirely on it.
                // This "override header" allows specifying a string which will be
                // the one evaluated against test cases below (e.g. "/t/log/levels"),
                // or in other words, a request header which will be here considered
                // the request's URL determining the test case.
                test_case = h;
                debug!(
                    "#{} overriding test case from Pwm-Test-Case header: \"{}\"",
                    self.context_id, test_case
                );
            } else {
                test_case = path;
            }
        }

        let _method: Method = self
            .get_http_request_header(":method")
            .unwrap()
            .parse()
            .unwrap();

        if test_case.starts_with("/t/echo/header/") {
            // /t/echo/header/{header_name: String}
            let header_name = test_case
                .split('/')
                .collect::<Vec<&str>>()
                .split_off(4)
                .pop()
                .unwrap();

            echo_header(self, header_name);
            return;
        }

        match test_case.as_str() {
            /* log */
            "/t/log/request_body" => test_log_request_body(self),
            "/t/log/levels" => test_log_levels(self),
            "/t/log/request_headers" => test_log_request_headers(self),
            "/t/log/request_path" => test_log_request_path(self),
            "/t/log/response_header" => test_log_response_header(self),
            "/t/log/response_headers" => test_log_response_headers(self),
            "/t/log/response_body" => {
                test_log_response_body(self, self.response_body_size.unwrap_or(30))
            }
            "/t/log/current_time" => test_log_current_time(self),

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
            "/t/set_http_request_headers" => test_set_http_request_headers(self),
            "/t/set_http_request_headers/special" => test_set_http_request_headers_special(self),
            "/t/set_http_response_headers" => test_set_http_response_headers(self),
            "/t/set_http_request_header" => test_set_http_request_header(self),
            "/t/set_http_response_header" => test_set_http_response_header(self),
            "/t/add_http_request_header" => test_add_http_request_header(self),
            "/t/add_http_response_header" => test_add_http_response_header(self),

            /* set/add request/response body */
            "/t/set_http_request_body" => test_set_http_request_body(self),
            "/t/set_http_response_body" => test_set_http_response_body(self),

            /* echo request */
            "/t/echo/headers" => echo_headers(self),
            "/t/echo/body" => echo_body(self, self.request_body_size),

            /* errors */
            "/t/error/get_response_body" => {
                let _body = self.get_http_response_body(usize::MAX, usize::MAX);
            }

            _ => (),
        }
    }
}

impl Context for TestHttpHostcalls {}
impl HttpContext for TestHttpHostcalls {
    fn on_http_request_headers(&mut self, nheaders: usize) -> Action {
        info!(
            "#{} on_request_headers, {} headers",
            self.context_id, nheaders
        );
        self.exec_tests(TestPhase::HttpRequestHeaders);
        Action::Continue
    }

    fn on_http_request_body(&mut self, size: usize, end_of_stream: bool) -> Action {
        self.request_body_size = Some(size);
        info!(
            "#{} on_request_body, {} bytes, end_of_stream: {}",
            self.context_id, size, end_of_stream
        );
        self.exec_tests(TestPhase::HttpRequestBody);
        Action::Continue
    }

    fn on_http_response_headers(&mut self, nheaders: usize) -> Action {
        info!(
            "#{} on_response_headers, {} headers",
            self.context_id, nheaders
        );
        self.exec_tests(TestPhase::HttpResponseHeaders);
        Action::Continue
    }

    fn on_http_response_body(&mut self, len: usize, end_of_stream: bool) -> Action {
        info!(
            "#{} on_response_body, {} bytes, end_of_stream {}",
            self.context_id, len, end_of_stream
        );
        if !end_of_stream || len > 0 {
            self.exec_tests(TestPhase::HttpResponseBody);
        }
        Action::Continue
    }

    fn on_log(&mut self) {
        info!("#{} on_log", self.context_id);
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
