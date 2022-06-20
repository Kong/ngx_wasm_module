#ifndef CWABT_H
#define CWABT_H

#include <wasm.h>

#include <stdbool.h>

/*
Reads a buffer containing textual .wat data, parses it,
and writes a buffer containing .wasm data.

Parameters:
* wat: pointer to a wasm_byte_vec_t containing the input in .wat format.
* wasm: pointer to a wasm_byte_vec_t that will contain the generated .wasm data.
  This needs to point to an allocated struct, but its given contents are
  ignored; the contents are filled by this function in case of a
  successful parse.

Return value:
* NULL in case of a successful parse, or a pointer to a newly allocated
  wasm_byte_vec_t containing the error message, owned by the caller.
*/
wasm_byte_vec_t* wat2wasm(const wasm_byte_vec_t *wat, wasm_byte_vec_t *wasm);

#endif /* CWABT_H */
