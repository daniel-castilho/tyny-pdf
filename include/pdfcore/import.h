#ifndef PDFCORE_IMPORT_H
#define PDFCORE_IMPORT_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

pc_status pc_backend_get_api(const pc_backend_api** out_api, uint32_t abi_major,
                             uint32_t abi_minor);

#ifdef __cplusplus
}
#endif

#endif