#![allow(clippy::single_match)]

mod filter;
mod tests;
mod types;

use crate::{tests::*, types::test_http::*, types::test_root::*, types::*};
use log::*;
use proxy_wasm::{traits::*, types::*};
use std::{collections::HashMap, time::Duration};

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(TestRoot {
            config: HashMap::new(),
        })
    });
}}

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

        if let Some(period) = self.get_config("tick_period") {
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
        info!(
            "[hostcalls] on_tick, {}",
            self.get_config("tick_period").unwrap()
        );

        match self.get_config("on_tick").unwrap_or("") {
            "log_property" => test_log_property(self),
            "set_property" => test_set_property(self),
            _ => (),
        }
    }

    fn create_http_context(&self, context_id: u32) -> Option<Box<dyn HttpContext>> {
        info!("create context id #{}", context_id);

        Some(Box::new(TestHttp {
            config: self.config.clone(),
            on_phase: self
                .config
                .get("on")
                .map_or(TestPhase::RequestHeaders, |s| {
                    s.parse()
                        .unwrap_or_else(|_| panic!("unknown phase: {:?}", s))
                }),
        }))
    }
}
