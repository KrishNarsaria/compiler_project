#ifndef BACKEND_H
#define BACKEND_H

#include <string>

/* Translates the three-address IR text into RISC-V (RV64IMFD) assembly for the
   RARS simulator.  When runtimeMode is true the output is the runtime library
   only (no entry stub, library label prefixes). */
bool generate_asm(const std::string &ir, const std::string &outfile, bool runtimeMode);

extern const char *g_runtime_asm;

#endif
