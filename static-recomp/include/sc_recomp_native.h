/* sc_recomp_native.h — T066 Phase 2b: native (register-allocated) blocks. */
#ifndef SC_RECOMP_NATIVE_H
#define SC_RECOMP_NATIVE_H

#include "sc_v11_runtime.h"

/* First native block: 0x059245..0x0592CC (group 0x59000), a straight-line
   initializer.  Returns 1 if the block ran to completion (including its COP
   terminator), 0 if it had to fall back to the interpreter. */
int sc_recomp_block_0x059245(SCV11Runtime *r);

#endif /* SC_RECOMP_NATIVE_H */