use crate::{tests::echo::*, tests::host::*, tests::*, *};
use http::StatusCode;
use log::*;
use proxy_wasm::{hostcalls::*, traits::*, types::*};

pub struct TestHttp {
    pub on_phases: Vec<TestPhase>,
    pub config: HashMap<String, String>,
    pub metrics: BTreeMap<String, u32>,
    pub n_sync_calls: usize,
    pub pending_callbacks: u32,
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
            /* /t/echo/header/{header_name: String} */
            let header_name = path
                .split('/')
                .collect::<Vec<&str>>()
                .remove(4)
                .split('?')
                .collect::<Vec<&str>>()
                .remove(0);

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
        if !self.on_phases.contains(&cur_phase) {
            return Action::Continue;
        }

        info!("[hostcalls] testing in \"{:?}\"", cur_phase);

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
            "/t/log/properties" => test_log_properties(self, cur_phase),
            "/t/log/metrics" => test_log_metrics(self, cur_phase),

            /* send_local_response */
            "/t/send_local_response/status/204" => test_send_status(self, 204),
            "/t/send_local_response/status/300" => test_send_status(self, 300),
            "/t/send_local_response/status/403" => test_send_status(self, 403),
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
                let n = self
                    .config
                    .get("ncalls")
                    .map_or(1, |v| v.parse().expect("bad ncalls value"));

                for i in 0..n {
                    self.send_http_dispatch(i);
                }

                return Action::Pause;
            }

            /* edge case: dispatch + local response */
            "/t/dispatch_and_local_response" => {
                self.send_http_dispatch(0);
                self.send_http_dispatch(0);
                test_send_status(self, 201)
            }

            /* shared memory */
            "/t/shm/get_shared_data" => test_get_shared_data(self),
            "/t/shm/log_shared_data" => test_log_shared_data(self),
            "/t/shm/set_shared_data" => test_set_shared_data(self),
            "/t/shm/set_shared_data_by_len" => test_set_shared_data_by_len(self),
            "/t/shm/enqueue" => test_shared_queue_enqueue(self),
            "/t/shm/dequeue" => test_shared_queue_dequeue(self),

            /* metrics */
            "/t/metrics/define" => test_define_metrics(self),
            "/t/metrics/increment_counters" => test_increment_counters(self, cur_phase, None),
            "/t/metrics/increment_gauges" => {
                let skip_non_counters = false;
                test_increment_counters(self, cur_phase, Some(skip_non_counters));
            }
            "/t/metrics/toggle_gauges" => test_toggle_gauges(self, cur_phase, None),
            "/t/metrics/toggle_counters" => {
                let skip_non_gauges = false;
                test_toggle_gauges(self, cur_phase, Some(skip_non_gauges));
            }
            "/t/metrics/record_histograms" => test_record_metric(self, cur_phase),
            "/t/metrics/get" => test_get_metrics(self),
            "/t/metrics/increment_invalid_counter" => increment_metric(0, 1).unwrap(),
            "/t/metrics/set_invalid_gauge" => record_metric(0, 1).unwrap(),
            "/t/metrics/get_invalid_metric" => {
                get_metric(0).unwrap();
            }
            "/t/metrics/get_histogram" => {
                info!("retrieving histogram in \"{:?}\"", cur_phase);
                let h_id =
                    define_metric(MetricType::Histogram, "h1").expect("cannot define new metric");
                get_metric(h_id).unwrap();
            }

            /* foreign functions */
            "/t/call_foreign_function" => {
                let name = self.config.get("name").expect("expected a name argument");

                let ret = match self.config.get("arg") {
                    Some(arg) => call_foreign_function(name, Some((&arg).as_bytes())).unwrap(),
                    None => call_foreign_function(name, None).unwrap(),
                }
                .unwrap();

                info!("foreign function {} returned {:?}", name, ret);
            }
            "/t/resolve_lua" => {
                self.pending_callbacks = test_proxy_resolve_lua(self);

                if self.pending_callbacks > 0 {
                    return Action::Pause;
                }
            }

            /* errors */
            "/t/trap" => panic!("custom trap"),
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
            "/t/bad_set_buffer_type" => test_set_buffer_bad_type(),
            "/t/bad_get_buffer_type" => test_get_buffer_bad_type(),
            "/t/bad_set_map_type" => test_set_map_bad_type(),
            "/t/bad_get_map_type" => test_get_map_bad_type(),
            "/t/bad_log_level" => test_bad_log_level(),
            "/t/nyi_host_func" => test_nyi_host_func(),
            _ => (),
        }

        Action::Continue
    }

    pub fn send_http_dispatch(&mut self, i: usize) {
        let mut timeout = Duration::from_secs(0);
        let mut headers = Vec::new();
        let mut path = self
            .config
            .get("path")
            .map(|v| v.as_str())
            .unwrap_or("/")
            .to_string();

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
            if self.n_sync_calls > 0 {
                path.push_str(format!("{}", self.n_sync_calls).as_str());
            }

            headers.push((":path", path.as_str()));
        }

        if self.get_config("authority").is_some() {
            headers.push((
                ":authority",
                self.config.get("authority").map(|v| v.as_str()).unwrap(),
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
            if let Ok(t) = val.parse::<u64>() {
                timeout = Duration::from_secs(t)
            }
        }

        headers.push(("Host", "ignoreme"));
        headers.push(("connection", "ignoreme"));

        let host = match self.get_config("hosts") {
            Some(list) => list.split(',').collect::<Vec<&str>>()[i],
            None => self.get_config("host").unwrap_or(""),
        };

        match self.dispatch_http_call(
            host,
            headers,
            self.get_config("body").map(|v| v.as_bytes()),
            vec![],
            timeout,
        ) {
            Ok(_) => {}
            Err(status) => match status {
                Status::BadArgument => error!("dispatch_http_call: bad argument"),
                Status::InternalFailure => error!("dispatch_http_call: internal failure"),
                status => panic!(
                    "dispatch_http_call: unexpected status \"{}\"",
                    status as u32
                ),
            },
        }
    }
}
