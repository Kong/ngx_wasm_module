use std::collections::HashMap;
use std::net::{Ipv4Addr, Ipv6Addr};

use log::{info, warn};
use proxy_wasm::traits::*;
use proxy_wasm::types::*;

proxy_wasm::main! {{
    proxy_wasm::set_log_level(LogLevel::Debug);
    proxy_wasm::set_root_context(|_| -> Box<dyn RootContext> {
        Box::new(Root { config: HashMap::new() })
    });
}}

struct Root {
    config: HashMap<String, String>,
}

impl Context for Root {}

impl RootContext for Root {
    fn on_configure(&mut self, _: usize) -> bool {
        if let Some(bytes) = self.get_plugin_configuration() {
            if let Ok(s) = String::from_utf8(bytes) {
                self.config = s
                    .split_whitespace()
                    .filter_map(|kv| kv.split_once('='))
                    .map(|(k, v)| (k.to_string(), v.to_string()))
                    .collect();
            }
        }
        true
    }

    fn create_http_context(&self, _: u32) -> Option<Box<dyn HttpContext>> {
        Some(Box::new(Http {
            host: self.config.get("host").cloned().unwrap_or_else(|| "localhost".to_string()),
            port: self.config
                .get("port")
                .and_then(|p| p.parse::<u32>().ok())
                .unwrap_or(8891),
            resolved_addr: None,
        }))
    }

    fn get_type(&self) -> Option<ContextType> {
        Some(ContextType::HttpContext)
    }
}

struct Http {
    host: String,
    port: u32,
    resolved_addr: Option<String>,
}

impl Context for Http {
    fn on_foreign_function(&mut self, function_id: u32, args_size: usize) {
        // function_id 1 = `resolve`
        if function_id != 1 {
            return;
        }

        let args = match proxy_wasm::hostcalls::get_buffer(
            BufferType::CallData, 0, args_size,
        ) {
            Ok(Some(a)) => a,
            _ => {
                warn!("upstream_resolve: async resolve returned no args");
                return;
            }
        };

        let address_size = args[0] as usize;
        let address = &args[1..address_size + 1];
        let name = std::str::from_utf8(&args[(address_size + 1)..]).unwrap_or("?");

        if address_size == 4 {
            let ip = Ipv4Addr::new(address[0], address[1], address[2], address[3]);
            info!("upstream_resolve: resolved {} to {} (async)", name, ip);
            self.resolved_addr = Some(ip.to_string());
        } else if address_size == 16 {
            let bytes: [u8; 16] = address.try_into().unwrap();
            let ip = Ipv6Addr::from(bytes);
            info!("upstream_resolve: resolved {} to {} (async)", name, ip);
            self.resolved_addr = Some(format!("[{}]", ip));
        } else {
            warn!("upstream_resolve: no address for {}", name);
        }
    }
}

impl HttpContext for Http {
    fn on_http_request_headers(&mut self, _: usize, _: bool) -> Action {
        match self.call_foreign_function("resolve", Some(self.host.as_bytes())) {
            Ok(Some(bytes)) => {
                if bytes.len() == 4 {
                    let ip = Ipv4Addr::new(bytes[0], bytes[1], bytes[2], bytes[3]);
                    info!("upstream_resolve: resolved {} to {} (sync)", self.host, ip);
                    self.resolved_addr = Some(ip.to_string());
                } else if bytes.len() == 16 {
                    let bs: [u8; 16] = bytes.try_into().unwrap();
                    let ip = Ipv6Addr::from(bs);
                    info!("upstream_resolve: resolved {} to {} (sync)", self.host, ip);
                    self.resolved_addr = Some(format!("[{}]", ip));
                } else {
                    warn!(
                        "upstream_resolve: sync resolve for {} returned {} bytes",
                        self.host, bytes.len()
                    );
                }
                Action::Continue
            }
            Ok(None) => {
                info!("upstream_resolve: yielded while resolving {}", self.host);
                Action::Pause
            }
            Err(e) => {
                info!("upstream_resolve: resolve \"{}\" rejected: {:?}", self.host, e);
                Action::Continue
            }
        }
    }

    fn on_http_upstream_select(&mut self) {
        if let Some(addr) = self.resolved_addr.clone() {
            info!("upstream_resolve: set_upstream {}:{}", addr, self.port);
            self.set_upstream(&addr, self.port, false, None);
        } else {
            warn!("upstream_resolve: no resolved address, skipping set_upstream");
        }
    }
}
