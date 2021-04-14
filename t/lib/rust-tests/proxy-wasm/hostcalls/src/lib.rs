mod echo;
mod test_cases;

use crate::{echo::*, test_cases::*};
use http::{Method, StatusCode};
use log::*;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;

struct TestHttpHostcalls {
    _context_id: u32,
}

impl TestHttpHostcalls {
    fn send_plain_response(&mut self, status: StatusCode, body: Option<String>) {
        if let Some(b) = body {
            self.send_http_response(status.as_u16() as u32, vec![], Some(b.as_bytes()))
        } else {
            self.send_http_response(status.as_u16() as u32, vec![], None)
        }
    }

    fn send_not_found(&mut self) {
        self.send_plain_response(StatusCode::NOT_FOUND, None)
    }

    fn exec_tests(&mut self) {
        let method: Method = self
            .get_http_request_header(":method")
            .unwrap()
            .parse()
            .unwrap();

        let path = self.get_http_request_header(":path").unwrap();
        if path.starts_with("/t/echo/header/") {
            let header_name = path
                .split('/')
                .collect::<Vec<&str>>()
                .split_off(4)
                .pop()
                .unwrap();

            echo_header(self, header_name);
            return;
        }

        match method {
            Method::GET => match path.as_str() {
                "/t/log/levels" => test_log_levels(self),
                "/t/log/current_time" => test_log_current_time(self),
                "/t/send_local_response/status/204" => test_send_status(self, 204),
                "/t/send_local_response/status/300" => test_send_status(self, 300),
                "/t/send_local_response/status/1000" => test_send_status(self, 1000),
                "/t/send_local_response/headers" => test_send_headers(self),
                "/t/send_local_response/body" => test_send_body(self),
                "/t/send_local_response/set_special_headers" => test_set_special_headers(self),
                "/t/send_local_response/set_headers_escaping" => test_set_headers_escaping(self),
                "/t/echo/headers" => echo_headers(self),
                _ => self.send_not_found(),
            },
            _ => self.send_plain_response(StatusCode::METHOD_NOT_ALLOWED, None),
        }
    }
}

impl RootContext for TestHttpHostcalls {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, _context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(TestHttpHostcalls { _context_id }))
    }
}

impl Context for TestHttpHostcalls {}
impl HttpContext for TestHttpHostcalls {
    fn on_http_request_headers(&mut self, nheaders: usize) -> Action {
        info!("number of request headers: {}", nheaders);
        self.exec_tests();
        Action::Continue
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestHttpHostcalls { _context_id: 0 })
    });
}
