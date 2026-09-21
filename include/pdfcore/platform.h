#ifndef PDFCORE_PLATFORM_H
#define PDFCORE_PLATFORM_H

#include <stdint.h>

#if defined(_WIN32)
#define fsync _commit
#define F_OK 0
#else
#include <unistd.h>
#endif

#endif