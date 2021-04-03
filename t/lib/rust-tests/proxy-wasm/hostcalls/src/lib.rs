mod test_cases;

use crate::test_cases::*;
use chrono::{DateTime, Utc};
use log::*;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestHttpHostcalls { context_id: 0 })
    });
}

struct TestHttpHostcalls {
    context_id: u32,
}

impl TestHttpHostcalls {
    fn exec(&self, uri: String) -> Action {
        /*
         * GET /t/<endpoint>/<path...>
         */
        let (endpoint, path);
        {
            let p = uri.strip_prefix("/t").unwrap_or_else(|| uri.as_str());
            let mut segments: Vec<&str> = p.split("/").collect::<Vec<&str>>().split_off(1);
            endpoint = segments.remove(0);
            path = segments.join("/");
        }

        match endpoint {
            "log" => test_log(path.as_str(), self),
            "log_current_time" => {
                let now: DateTime<Utc> = self.get_current_time().into();
                info!("now: {}", now)
            }
            "log_http_request_headers" => test_log_http_request_headers(path.as_str(), self),
            "send_http_response" => test_send_http_response(path.as_str(), self),
            "get_http_request_header" => test_get_http_request_header(path.as_str(), self),
            _ => self.send_http_response(404, vec![], None),
        }

        Action::Continue
    }
}

impl RootContext for TestHttpHostcalls {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(TestHttpHostcalls { context_id }))
    }
}

impl Context for TestHttpHostcalls {}

impl HttpContext for TestHttpHostcalls {
    fn on_http_request_headers(&mut self, _: usize) -> Action {
        match self.get_http_request_header(":path") {
            Some(path) => self.exec(path),
            _ => Action::Continue,
        }
    }
}
