use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::collections::HashMap;
use std::time::Duration;

#[derive(Debug, Eq, PartialEq, enum_utils::FromStr)]
#[enumeration(rename_all = "snake_case")]
enum Phase {
    RequestHeaders,
    RequestBody,
    RequestTrailers,
    ResponseHeaders,
    ResponseBody,
    ResponseTrailers,
    Log,
}

const ROOT_ID: u32 = 0;

struct HttpHeadersRoot {
    config: HashMap<String, String>,
}

impl Context for HttpHeadersRoot {}
impl RootContext for HttpHeadersRoot {
    fn on_vm_start(&mut self, config_size: usize) -> bool {
        info!("on_vm_start, config_size: {}", config_size);
        true
    }

    fn on_configure(&mut self, config_size: usize) -> bool {
        info!("#{} on_configure, config_size: {}", ROOT_ID, config_size);

        if let Some(config_bytes) = self.get_plugin_configuration() {
            assert!(config_bytes.len() == config_size);

            let config_str = String::from_utf8(config_bytes).unwrap();
            self.config = config_str
                .split_whitespace()
                .filter_map(|s| s.split_once('='))
                .map(|(k, v)| (k.to_string(), v.to_string()))
                .collect();

            info!("#{} config: {}", ROOT_ID, config_str);

            if self.config.contains_key("fail_configure") {
                return false;
            }

            if let Some(period) = self.config.get("tick_period") {
                self.set_tick_period(Duration::from_millis(
                    period.parse().expect("bad tick_period"),
                ));
            }
        }

        true
    }

    fn on_tick(&mut self) {
        info!("on_tick {}", self.config.get("tick_period").unwrap());
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(OnPhases {
            context_id,
            config: self.config.clone(),
            pause_on: self
                .config
                .get("pause_on")
                .map(|s| s.parse().expect("bad pause_on")),
        }))
    }
}

struct OnPhases {
    context_id: u32,
    config: HashMap<String, String>,
    pause_on: Option<Phase>,
}

impl OnPhases {
    fn next_action(&mut self, phase: Phase) -> Action {
        if let Some(pause_on) = &self.pause_on {
            if pause_on == &phase {
                info!(
                    "#{} pausing after \"{:?}\" phase",
                    self.context_id, pause_on
                );

                return Action::Pause;
            }
        }

        Action::Continue
    }
}

impl Context for OnPhases {}
impl HttpContext for OnPhases {
    fn on_http_request_headers(&mut self, nheaders: usize, _eof: bool) -> Action {
        info!(
            "#{} on_request_headers, {} headers",
            self.context_id, nheaders
        );

        self.next_action(Phase::RequestHeaders)
    }

    fn on_http_request_body(&mut self, len: usize, end_of_stream: bool) -> Action {
        info!(
            "#{} on_request_body, {} bytes, end_of_stream: {}",
            self.context_id, len, end_of_stream
        );

        self.next_action(Phase::RequestBody)
    }

    fn on_http_response_headers(&mut self, nheaders: usize, _eof: bool) -> Action {
        info!(
            "#{} on_response_headers, {} headers",
            self.context_id, nheaders
        );

        self.next_action(Phase::ResponseHeaders)
    }

    fn on_http_response_body(&mut self, len: usize, end_of_stream: bool) -> Action {
        info!(
            "#{} on_response_body, {} bytes, end_of_stream: {}",
            self.context_id, len, end_of_stream
        );

        self.next_action(Phase::ResponseBody)
    }

    fn on_log(&mut self) {
        info!("#{} on_log", self.context_id);

        let log_msg = self
            .config
            .get("log_msg")
            .map_or(String::new(), |s| s.to_string());

        if !log_msg.is_empty() {
            info!("#{} log_msg: {}", self.context_id, log_msg);
        }
    }
}

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Debug);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(HttpHeadersRoot {
            config: HashMap::new(),
        })
    });
}
