#![allow(clippy::single_match)]

mod filter;
mod tests;
mod types;

use crate::{tests::*, types::test_http::*, types::test_root::*, types::*};
use log::*;
use proxy_wasm::{traits::*, types::*};
use std::{collections::BTreeMap, collections::HashMap, time::Duration};

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestRoot {
            config: HashMap::new(),
            metrics: BTreeMap::new(),
            n_sync_calls: 0,
        })
    });
}}

impl RootContext for TestRoot {
    fn on_vm_start(&mut self, _: usize) -> bool {
        info!("[hostcalls] #0 on_vm_start");

        if let Some(config) = self.get_vm_configuration() {
            if let Ok(text) = std::str::from_utf8(&config) {
                info!("vm config: {}", text);

                match text {
                    "do_trap" => panic!("trap on_vm_start"),
                    "do_false" => {
                        info!("on_vm_start returning false");
                        return false;
                    }
                    _ => (),
                }
            } else {
                info!("cannot parse vm config");
            }
        } else {
            info!("no vm config");
        }

        true
    }

    fn on_configure(&mut self, _: usize) -> bool {
        info!("[hostcalls] #0 on_configure");

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

        if let Some(period) = self.get_config("tick_period") {
            self.set_tick_period(Duration::from_millis(
                period.parse().expect("bad tick_period"),
            ));
        }

        match self.get_config("on_configure").unwrap_or("") {
            "do_trap" => panic!("trap on_configure"),
            "do_return_false" => return false,
            "define_metrics" => test_define_metrics(self),
            "define_and_increment_counters" => {
                test_define_metrics(self);
                test_increment_counters(self, TestPhase::Configure, None);
            }
            "define_and_toggle_gauges" => {
                test_define_metrics(self);
                test_toggle_gauges(self, TestPhase::Configure, None);
            }
            "define_and_record_histograms" => {
                test_define_metrics(self);
                test_record_metric(self, TestPhase::Configure);
            }
            _ => (),
        }

        true
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }

    fn on_tick(&mut self) {
        info!(
            "[hostcalls] on_tick, {}",
            self.get_config("tick_period").unwrap()
        );

        let n_sync_calls = self
            .config
            .get("n_sync_calls")
            .map_or(1, |v| v.parse().expect("bad n_sync_calls value"));

        if self.n_sync_calls >= n_sync_calls {
            return;
        }

        match self.get_config("on_tick").unwrap_or("") {
            "log_property" => test_log_property(self),
            "set_gauges" => {
                test_record_metric(self, TestPhase::Tick);
                self.n_sync_calls += 1;
            }
            "set_property" => test_set_property(self),
            "dispatch" => {
                let mut timeout = Duration::from_secs(0);
                let mut headers = Vec::new();
                let path = self
                    .config
                    .get("path")
                    .map(|v| v.as_str())
                    .unwrap_or("/")
                    .to_string();

                if let Some(val) = self.get_config("timeout") {
                    if let Ok(t) = val.parse::<u64>() {
                        timeout = Duration::from_secs(t)
                    }
                }

                headers.push((":path", path.as_str()));
                headers.push((
                    ":method",
                    self.config
                        .get("method")
                        .map(|v| v.as_str())
                        .unwrap_or("GET"),
                ));

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

                self.n_sync_calls += 1;

                info!("call {}", self.n_sync_calls);

                self.dispatch_http_call(
                    self.get_config("host").unwrap_or(""),
                    headers,
                    self.get_config("body").map(|v| v.as_bytes()),
                    vec![],
                    timeout,
                )
                .unwrap();
            }
            _ => (),
        }
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        info!("create context id #{}", context_id);
        let mut phases: Vec<TestPhase>;

        match self.config.get("on") {
            Some(s) => {
                phases = vec![];
                for phase_name in s.split(',') {
                    phases.push(
                        phase_name
                            .parse()
                            .unwrap_or_else(|_| panic!("unknown phase: {:?}", phase_name)),
                    );
                }
            }
            None => phases = vec![TestPhase::RequestHeaders],
        }

        Some(Box::new(TestHttp {
            config: self.config.clone(),
            on_phases: phases,
            metrics: self.metrics.clone(),
            n_sync_calls: 0,
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
            "[hostcalls] on_root_http_call_response (id: {}, status: {}, headers: {}, body_bytes: {}, trailers: {})",
            token_id, status.unwrap_or("".to_string()), nheaders, body_size, ntrailers
        );
    }
}
