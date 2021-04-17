# Contributing

[Pending]

Contribution guidelines have not been drafted yet, but below is a list of
resources that can already be helpful in getting involved.

## Roadmap

This module's roadmap is documented via [GitHub
projects](https://github.com/Kong/ngx_wasm_module/projects).

A non-exhaustive list of ideas WasmX aims to explore:

- Improve nginx's telemetry reporting.
- Improve nginx's hot-reloading capabilities.
- Load Wasm bytecode at runtime (consider a dedicated nginx process type?).
- Support for Envoy's [xDS
  API](https://www.envoyproxy.io/docs/envoy/latest/api/api_supported_versions).
- Consider multiplexing & routing connections at a lower level than currently
  offered by stock nginx modules.
- Consider a build process with [Bazel](https://bazel.build/).

## TODOs

List the annotated "TODOs" in the sources and test cases with:

```
$ make todo
```

## Building from source

See [DEVELOPER.md](DEVELOPER.md) for developer resources on building this module
from source and other general development processes.
