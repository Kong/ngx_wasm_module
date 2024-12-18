use log::*;

pub fn resolve_lua_callback(args: Option<Vec<u8>>) {
    match args {
        Some(args) => {
            let address_size = args[0] as usize;
            let name = std::str::from_utf8(&args[(address_size + 1)..]).unwrap();

            if address_size > 0 {
                let address = &args[1..address_size + 1];
                info!("resolved (yielding) {} to {:?}", name, address);
            } else {
                info!("could not resolve {}", name)
            }
        }
        _ => info!("empty args")
    }
}
