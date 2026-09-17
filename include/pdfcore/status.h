#ifndef PDFCORE_STATUS_H
#define PDFCORE_STATUS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_status {
    uint32_t size;
    uint32_t code;
    uint32_t detail_id;
    const char *detail;
} pc_status;

#define PC_STATUS_INIT { sizeof(pc_status), 0, 0, NULL }

typedef enum pc_error {
    PC_ERR_NONE = 0,
    PC_ERR_ARGUMENT = 1,
    PC_ERR_UNSUPPORTED = 2,
    PC_ERR_BACKEND = 3,
    PC_ERR_CORRUPT = 4,
    PC_ERR_IO = 5,
    PC_ERR_MEMORY = 6,
    PC_ERR_CRYPTO = 7,
    PC_ERR_PERMISSION = 8,
    PC_ERR_DAMAGED = 9,
    PC_ERR_PASSWORD = 10,
    PC_ERR_RANGE = 11,
    PC_ERR_LIMIT = 12,
    PC_ERR_STATE = 13,
    PC_ERR_VERSION = 14,
    PC_ERR_FEATURE = 15,
} pc_error;

static inline int pc_status_is_ok(const pc_status *s) {
    return s && s->code == PC_ERR_NONE;
}

static inline void pc_status_set(pc_status *s, uint32_t code, uint32_t detail_id, const char *detail) {
    if (!s) return;
    s->size = sizeof(pc_status);
    s->code = code;
    s->detail_id = detail_id;
    s->detail = detail;
}

#ifdef __cplusplus
}
#endif

#endif