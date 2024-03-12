use crate::*;

pub struct TestRoot {
    pub config: HashMap<String, String>,
    pub metrics: HashMap<String, u32>,
    pub n_sync_calls: usize,
}
