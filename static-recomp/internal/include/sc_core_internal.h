#ifndef SC_CORE_INTERNAL_H
#define SC_CORE_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "sc_machine.h"

#define SC_EXPECTED_RESET_VECTOR 0x8000u

void sc_copy_text(char *out, size_t cap, const char *text);
void sc_sha256_bytes(const unsigned char *data, size_t size, char out_hex[65]);

/* True while a process-wide static-core log is open.  Frame hashes are
   diagnostic metadata with no consumer outside the renderer, so the renderer
   skips them (about 3 ms per frame at 398x239) unless logging is active. */
int sc_core_logging_enabled(void);
uint32_t sc_crc32_bytes(const unsigned char *data, size_t size);

#endif
