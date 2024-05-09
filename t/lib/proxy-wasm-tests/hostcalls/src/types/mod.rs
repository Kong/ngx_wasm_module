pub mod test_http;
pub mod test_root;

use crate::*;

#[derive(Debug, Eq, PartialEq, enum_utils::FromStr)]
#[enumeration(rename_all = "snake_case")]
pub enum TestPhase {
    Configure,
    Tick,
    RequestHeaders,
    RequestBody,
    ResponseHeaders,
    ResponseBody,
    ResponseTrailers,
    Log,
}

pub trait TestContext {
    fn get_config(&self, name: &str) -> Option<&str>;
    fn get_metrics_mapping(&self) -> &BTreeMap<String, u32>;
    fn save_metric_mapping(&mut self, name: &str, metric_id: u32) -> Option<u32>;
}

impl Context for dyn TestContext {}

impl TestContext for TestRoot {
    fn get_config(&self, name: &str) -> Option<&str> {
        self.config.get(name).map(|s| s.as_str())
    }

    fn get_metrics_mapping(&self) -> &BTreeMap<String, u32> {
        &self.metrics
    }

    fn save_metric_mapping(&mut self, name: &str, metric_id: u32) -> Option<u32> {
        self.metrics.insert(name.to_string(), metric_id)
    }
}

impl TestContext for TestHttp {
    fn get_config(&self, name: &str) -> Option<&str> {
        self.config.get(name).map(|s| s.as_str())
    }

    fn get_metrics_mapping(&self) -> &BTreeMap<String, u32> {
        &self.metrics
    }

    fn save_metric_mapping(&mut self, name: &str, metric_id: u32) -> Option<u32> {
        self.metrics.insert(name.to_string(), metric_id)
    }
}
