#include <v8.h>
#include "bridge.h"

extern "C" int ngx_v8_enable_wasm_trap_handler(int use_v8_signal_handler) {
    return v8::V8::EnableWebAssemblyTrapHandler(use_v8_signal_handler);
}
