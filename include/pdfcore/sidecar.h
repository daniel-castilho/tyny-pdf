#ifndef PDFCORE_SIDECAR_H
#define PDFCORE_SIDECAR_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_sidecar pc_sidecar;

/// Staleness reason codes
typedef enum pc_staleness_reason {
  PC_STALE_NONE = 0,
  PC_STALE_PAGE_COUNT = 1,     // page count mismatch (R5.1)
  PC_STALE_FINGERPRINT = 2,    // document fingerprint changed (R5.2 strong signal)
  PC_STALE_MODIFIED_TIME = 3,  // modification time changed (R5.2 weak signal)
} pc_staleness_reason;

struct pc_sidecar {
  int format_version;
  char* document_sha256;  // stored fingerprint
  char* document_path;
  int64_t modified_time;  // stored modification time
  int page_count;         // stored page count
  char* annotations_json;
  char* unknown_json;
  int is_stale;                      // 1 if stale detected
  pc_staleness_reason stale_reason;  // why it's stale
};

/// Write sidecar atomically (R3.1 tmp->rename + fsync, R3.2 lock, R4.1 unknown preserve, R6
/// exclusions) Returns PC_ERR_NONE on success, PC_ERR_ARGUMENT if path mismatch (R4.2),
/// PC_ERR_STATE if locked
pc_status pc_sidecar_write(const char* doc_path, const pc_sidecar* sidecar);

/// Read sidecar; populates is_stale/stale_reason by checking current document (R5.1, R5.2)
/// Returns PC_ERR_NONE on success, PC_ERR_IO if not found
/// Caller owns *out_sidecar, must free with pc_sidecar_free
pc_status pc_sidecar_read(const char* doc_path, pc_sidecar** out_sidecar);

/// Free sidecar allocated by pc_sidecar_read
void pc_sidecar_free(pc_sidecar* sidecar);

/// Acquire .tynypdf.lock for 5-minute exclusive write window (R3.2)
/// Returns PC_ERR_NONE if acquired, PC_ERR_STATE if lock exists and is fresh
pc_status pc_sidecar_try_lock(const char* doc_path);

/// Release .tynypdf.lock
void pc_sidecar_unlock(const char* doc_path);

/// Validate an annotation ID per RFC 4648 (lowercase base32, 10 chars, no padding) (R2.3)
/// Returns PC_ERR_NONE if valid, PC_ERR_ARGUMENT if invalid
pc_status pc_sidecar_validate_annotation_id(const char* annotation_id);

/// Check staleness of sidecar against current document (R5.1, R5.2)
/// Sets is_stale and stale_reason on the sidecar
/// Returns PC_ERR_NONE on success, PC_ERR_IO if document not accessible
pc_status pc_sidecar_check_staleness(const char* doc_path, pc_sidecar* sidecar);

/// Compute SHA256 fingerprint of document (R5.2 strong signal)
/// Returns PC_ERR_NONE on success, out_fingerprint must be freed by caller
pc_status pc_sidecar_compute_fingerprint(const char* doc_path, char** out_fingerprint);

/// Compute page count of document (R5.1)
/// Returns PC_ERR_NONE on success
pc_status pc_sidecar_compute_page_count(const char* doc_path, int* out_page_count);

#ifdef __cplusplus
}
#endif

#endif