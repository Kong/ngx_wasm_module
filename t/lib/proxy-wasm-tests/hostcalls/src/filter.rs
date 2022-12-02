use crate::{test_http::*, types::*};
use http::StatusCode;
use log::*;
use proxy_wasm::{traits::*, types::*};

impl Context for TestHttp {
    fn on_http_call_response(
        &mut self,
        token_id: u32,
        nheaders: usize,
        body_size: usize,
        ntrailers: usize,
    ) {
        let bytes = self.get_http_call_response_body(0, body_size);
        let op = self
            .config
            .get("on_http_call_response")
            .map_or("", |v| v.as_str());

        info!(
            "[hostcalls] on_http_call_response (id: {}, headers: {}, body_bytes: {}, trailers: {}, op: {})",
            token_id, nheaders, body_size, ntrailers, op
        );

        match op {
            "trap" => panic!("trap!"),
            "echo_response_body" => {
                if let Some(response) = bytes {
                    let body = String::from_utf8_lossy(&response);
                    self.send_plain_response(StatusCode::OK, Some(body.trim()));
                }
            }
            "log_request_properties" => {
                let properties = vec![
                    "request.path",
                    "request.url_path",
                    "request.host",
                    "request.scheme",
                    "request.method",
                    "request.time",
                    "request.protocol",
                    "request.query",
                    "request.total_size",
                ];

                for property in properties {
                    match self.get_property(property.split('.').collect()) {
                        Some(p) => match std::str::from_utf8(&p) {
                            Ok(value) => {
                                info!("{}: {} at OnHttpCallResponse", property, value);
                            }
                            Err(_) => panic!(
                                "failed converting {} to UTF-8 at OnHttpCallResponse",
                                property
                            ),
                        },
                        None => info!("{}: not-found at OnHttpCallResponse", property),
                    }
                }

                if let Some(response) = bytes {
                    let body = String::from_utf8_lossy(&response);
                    self.send_plain_response(StatusCode::OK, Some(body.trim()));
                }
            }
            "echo_response_headers" => {
                let headers = self.get_http_call_response_headers();
                let mut s = String::new();
                for (k, v) in headers {
                    s.push_str(&k);
                    s.push_str(": ");
                    s.push_str(&v);
                    s.push('\n');
                }
                self.send_plain_response(StatusCode::OK, Some(&s));
            }
            "call_again" => {
                if let Some(response) = bytes {
                    let body = String::from_utf8_lossy(&response);
                    self.add_http_response_header(
                        format!("pwm-call-{}", self.ncalls + 1).as_str(),
                        body.trim(),
                    );
                }

                let again = self
                    .config
                    .get("call_again")
                    .map_or(1, |v| v.parse().expect("bad call_again value"));

                if self.ncalls < again {
                    self.ncalls += 1;
                    self.send_http_dispatch();
                    return;
                }

                self.send_plain_response(
                    StatusCode::OK,
                    Some(format!("called {} times", self.ncalls + 1).as_str()),
                );
            }
            _ => {}
        }

        self.resume_http_request()
    }
}

impl HttpContext for TestHttp {
    fn on_http_request_headers(&mut self, nheaders: usize, eof: bool) -> Action {
        info!(
            "[hostcalls] on_request_headers, {} headers, eof: {}",
            nheaders, eof
        );

        self.exec_tests(TestPhase::RequestHeaders)
    }

    fn on_http_request_body(&mut self, size: usize, eof: bool) -> Action {
        info!("[hostcalls] on_request_body, {} bytes, eof: {}", size, eof);
        self.exec_tests(TestPhase::RequestBody)
    }

    fn on_http_response_headers(&mut self, nheaders: usize, eof: bool) -> Action {
        info!(
            "[hostcalls] on_response_headers, {} headers, eof: {}",
            nheaders, eof
        );

        self.exec_tests(TestPhase::ResponseHeaders)
    }

    fn on_http_response_body(&mut self, len: usize, eof: bool) -> Action {
        info!("[hostcalls] on_response_body, {} bytes, eof: {}", len, eof);
        self.exec_tests(TestPhase::ResponseBody)
    }

    fn on_http_response_trailers(&mut self, ntrailers: usize) -> Action {
        info!("[hostcalls] on_response_trailers, {} trailers", ntrailers);
        self.exec_tests(TestPhase::ResponseTrailers)
    }

    fn on_log(&mut self) {
        info!("[hostcalls] on_log");
        self.exec_tests(TestPhase::Log);
    }
}
