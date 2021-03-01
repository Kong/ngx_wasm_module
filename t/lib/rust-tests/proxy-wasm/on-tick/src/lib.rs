use chrono::{DateTime, Utc};
use log::info;
use proxy_wasm::traits::*;
use proxy_wasm::types::*;
use std::time::Duration;

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Debug);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> { Box::new(OnTick) });
}

struct OnTick;

impl Context for OnTick {}

impl RootContext for OnTick {
    fn on_vm_start(&mut self, _: usize) -> bool {
        self.set_tick_period(Duration::from_millis(100));
        true
    }

    fn on_tick(&mut self) {
        let now: DateTime<Utc> = self.get_current_time().into();
        info!("Ticking at {}", now);
    }
}
