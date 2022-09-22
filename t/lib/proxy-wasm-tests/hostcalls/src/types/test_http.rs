use crate::{tests::echo::*, tests::*, *};
use http::StatusCode;
use log::*;
use parse_duration::parse;
use proxy_wasm::{traits::*, types::*};
use std::time::Duration;
use url::Url;

pub struct TestHttp {
    pub on_phase: TestPhase,
    pub config: HashMap<String, String>,
}

impl TestHttp {
    pub fn send_plain_response(&mut self, status: StatusCode, body: Option<&str>) {
        self.send_http_response(status.as_u16() as u32, vec![], body.map(|b| b.as_bytes()))
    }

    pub fn _send_not_found(&mut self) {
        self.send_plain_response(StatusCode::NOT_FOUND, None)
    }

    fn serve_echo(&mut self, path: &str) {
        if path.starts_with("/t/echo/header/") {
            let url = Url::options()
                .base_url(Some(&Url::parse("http://localhost").unwrap()))
                .parse(path)
                .expect("bad path");

            /* /t/echo/header/{header_name: String} */
            let header_name = url
                .path()
                .split('/')
                .collect::<Vec<&str>>()
                .split_off(4)
                .pop()
                .unwrap();

            echo_header(self, header_name);
        }

        match self.config.get("test").unwrap_or(&path.into()).as_str() {
            "/t/echo/headers" => echo_headers(self),
            "/t/echo/headers/all" => echo_all_headers(self),
            "/t/echo/body" => echo_body(self),
            _ => (),
        }
    }

    pub fn exec_tests(&mut self, cur_phase: TestPhase) -> Action {
        if cur_phase != self.on_phase {
            return Action::Continue;
        }

        info!("[hostcalls] testing in \"{:?}\"", self.on_phase);

        let path = self.get_http_request_header(":path").unwrap();

        self.serve_echo(path.as_str());

        match self.config.get("test").unwrap_or(&path).as_str() {
            /* log */
            "/t/log/levels" => test_log_levels(self),
            "/t/log/current_time" => test_log_current_time(self),
            "/t/log/request_path" => test_log_request_path(self),
            "/t/log/request_header" => test_log_request_header(self),
            "/t/log/request_headers" => test_log_request_headers(self),
            "/t/log/request_body" => test_log_request_body(self),
            "/t/log/response_header" => test_log_response_header(self),
            "/t/log/response_headers" => test_log_response_headers(self),
            "/t/log/response_body" => test_log_response_body(self),
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
            "/t/set_request_header" => test_set_request_header(self),
            "/t/set_response_header" => test_set_response_header(self),
            "/t/add_request_header" => test_add_request_header(self),
            "/t/add_response_header" => test_add_response_header(self),

            /* set/add request/response body */
            "/t/set_request_body" => test_set_request_body(self),
            "/t/set_response_body" => test_set_response_body(self),

            /* set property */
            "/t/set_property" => test_set_property(self),

            /* http dispatch */
            "/t/dispatch_http_call" => {
                self.send_http_dispatch();
                return Action::Pause;
            }

            /* errors */
            "/t/trap" => panic!("trap"),
            "/t/error/get_response_body" => {
                let _body = self.get_http_response_body(usize::MAX, usize::MAX);
            }
            "/t/safety/proxy_get_header_map_value_oob_key" => {
                test_proxy_get_header_map_value_oob_key(self)
            }
            "/t/safety/proxy_get_header_map_value_oob_return_data" => {
                test_proxy_get_header_map_value_oob_return_data(self)
            }
            "/t/safety/proxy_get_header_map_value_misaligned_return_data" => {
                test_proxy_get_header_map_value_misaligned_return_data(self)
            }
            _ => (),
        }

        Action::Continue
    }

    pub fn send_http_dispatch(&mut self) {
        let mut timeout = Duration::from_secs(0);
        let mut headers = Vec::new();

        if self.get_config("no_method").is_none() {
            headers.push((
                ":method",
                self.config
                    .get("method")
                    .map(|v| v.as_str())
                    .unwrap_or("GET"),
            ));
        }

        if self.get_config("method_prefixed_header") == Some("yes") {
            headers.push((
                ":methodx",
                self.config
                    .get("method")
                    .map(|v| v.as_str())
                    .unwrap_or("GET"),
            ));
        }

        if self.get_config("no_path").is_none() {
            headers.push((
                ":path",
                self.config.get("path").map(|v| v.as_str()).unwrap_or("/"),
            ));
        }

        if self.get_config("no_authority").is_none() {
            headers.push((
                ":authority",
                self.config
                    .get("host")
                    .map(|v| v.as_str())
                    .unwrap_or("default"),
            ));
        }

        if self.get_config("https") == Some("yes") {
            headers.push((":scheme", "https"));
        } else {
            headers.push((":scheme", "http"));
        }

        if let Some(vals) = self.get_config("headers") {
            for (k, v) in vals.split('|').filter_map(|s| s.split_once(':')) {
                headers.push((k, v));
            }
        }

        if let Some(val) = self.get_config("timeout") {
            if let Ok(t) = parse(val) {
                timeout = t
            }
        }

        self.dispatch_http_call(
            self.get_config("host").unwrap_or(""),
            headers,
            self.get_config("body").map(|v| v.as_bytes()),
            vec![],
            timeout,
        )
        .unwrap();
    }
}
