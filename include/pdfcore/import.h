#ifndef PDFCORE_IMPORT_H
#define PDFCORE_IMPORT_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Resolve the versioned backend vtable (R-M3). A backend built against a newer abi_major than
/// abi_major is refused: PC_ERR_VERSION with a message naming both versions. PC_ERR_ARGUMENT for
/// null out_api.
pc_status pc_backend_get_api(const pc_backend_api** out_api, uint32_t abi_major,
                             uint32_t abi_minor);

#ifdef __cplusplus
}
#endif

#endif