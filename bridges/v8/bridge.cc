#include <v8.h>
#include "bridge.h"

extern "C" int ngx_v8_enable_wasm_trap_handler(int use_v8_signal_handler) {
    //v8::V8::SetFlagsFromString("--perf-prof --perf-prof-annotate-wasm --no-liftoff");
    //v8::V8::SetFlagsFromString("--no-liftoff");
    //v8::V8::SetFlagsFromString("--liftoff-only");
    return v8::V8::EnableWebAssemblyTrapHandler(use_v8_signal_handler);
}
