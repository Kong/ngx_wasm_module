# Contributing

[Pending]

## TODOs

Contribution guidelines have not been drafted yet, but here is a list of things
that are currently being worked on or should eventually receive attention:

- Test the building process on as many platforms as possible (see
  [DEVELOPER.md](DEVELOPER.md)).
- Add [Wasmer](https://github.com/wasmerio/wasmer) support to the CI tests
  matrix (see Wasmtime's [action.yml](.github/actions/wasmtime/action.yml)).
- Support for building ngx_wasm_module as a shared object (i.e. "dynamic
  module").
- Finishing the [proxy-wasm](https://github.com/proxy-wasm/spec) HTTP specification.
- Consider a build process with [Bazel](https://bazel.build/).
- Implement configuration directives for the Wasm runtimes.
- Implement a `wasm_log` directive with Wasm-only logs.
- Load Wasm bytecode at runtime (consider a dedicated nginx process type?).

List the annotated "TODOs" in the sources and test cases with:

```
$ make todo
```

## Building from source

See [DEVELOPER.md](DEVELOPER.md) for developer resources on building this module
from source and other general development processes.
