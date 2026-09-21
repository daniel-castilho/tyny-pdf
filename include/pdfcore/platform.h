#ifndef PDFCORE_PLATFORM_H
#define PDFCORE_PLATFORM_H

#include <stdint.h>

/// The sole OS seam in the public surface (ADR-0002). Everything else travels through the
/// win32/linux trees under src/os.
#if defined(_WIN32)
#define fsync _commit
#define F_OK 0
#else
#include <unistd.h>
#endif

#endif
