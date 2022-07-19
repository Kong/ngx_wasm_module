#ifndef _NGX_WRT_V8_BRIDGE_H_INCLUDED_
#define _NGX_WRT_V8_BRIDGE_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

int ngx_v8_enable_wasm_trap_handler(int use_v8_signal_handler);
void ngx_v8_set_flags(const char *flags);

#ifdef __cplusplus
}
#endif

#endif /* _NGX_WRT_V8_BRIDGE_H_INCLUDED_ */
