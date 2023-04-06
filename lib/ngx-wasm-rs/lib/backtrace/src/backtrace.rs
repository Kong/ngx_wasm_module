use std::convert::TryInto;

// see specification at:
// https://webassembly.github.io/spec/core/appendix/custom.html#name-section
fn read_name_section(
    mut reader: wasmparser::BinaryReader,
) -> Result<Vec<(u32, String)>, wasmparser::BinaryReaderError> {
    let mut table: Vec<(u32, String)> = Vec::new();

    while !reader.eof() {
        let id = reader.read_u8()?;
        let size = reader.read_var_u32()?;

        if id != 1 {
            reader.read_bytes(size.try_into().unwrap())?;
            continue;
        }

        let len = reader.read_var_u32()?;

        for _ in 0..len {
            let idx = reader.read_var_u32()?;
            let name = reader.read_string()?;

            table.push((idx, name.to_string()));
        }
    }

    Ok(table)
}

pub(crate) fn get_function_name_table(
    wasm_slice: &[u8],
) -> Result<Vec<(u32, String)>, wasmparser::BinaryReaderError> {
    for payload in wasmparser::Parser::new(0).parse_all(wasm_slice).flatten() {
        if let wasmparser::Payload::CustomSection(s) = payload {
            if s.name() == "name" {
                let reader = wasmparser::BinaryReader::new(s.data());
                return read_name_section(reader);
            }
        }
    }

    Ok(Vec::new())
}

pub(crate) fn demangle(mangled: &str) -> String {
    rustc_demangle::demangle(mangled).to_string()
}
