mod test_cases;

use crate::test_cases::*;
use chrono::{DateTime, Utc};
use log::*;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> { Box::new(HttpHeadersRoot) });
}

struct HttpHeadersRoot;

impl Context for HttpHeadersRoot {}

impl RootContext for HttpHeadersRoot {
    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(HttpHeaders { context_id }))
    }
}

struct HttpHeaders {
    context_id: u32,
}

impl Context for HttpHeaders {}

impl HttpContext for HttpHeaders {
    fn on_http_request_headers(&mut self, _: usize) -> Action {
        match self.get_http_request_header(":path") {
            Some(path) => {
                /*
                 * GET /t/<module>/<test_case>
                 */
                let (module, test);
                {
                    let p = path.strip_prefix("/t").unwrap_or_else(|| path.as_str());
                    let mut segments: Vec<&str> = p.split("/").collect::<Vec<&str>>().split_off(1);
                    module = segments.remove(0);
                    test = segments.join("/");
                }

                match module {
                    "log" => test_log(test.as_str(), self),
                    "get_current_time" => {
                        let now: DateTime<Utc> = self.get_current_time().into();
                        info!("now: {}", now)
                    }
                    "send_http_response" => test_send_http_response(test.as_str(), self),
                    "get_http_request_headers" => {
                        test_get_http_request_headers(test.as_str(), self)
                    }
                    "get_http_request_header" => test_get_http_request_header(test.as_str(), self),
                    _ => self.send_http_response(404, vec![], None),
                }

                Action::Continue
            }
            _ => Action::Continue,
        }
    }
}
