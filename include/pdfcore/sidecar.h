#ifndef PDFCORE_SIDECAR_H
#define PDFCORE_SIDECAR_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_sidecar pc_sidecar;

/// The highest format_version this build can mutate. A sidecar declaring a higher
/// version is loaded read-only and pc_sidecar_read returns PC_ERR_VERSION (R2.2).
#define PC_SIDECAR_FORMAT_VERSION_SUPPORTED 1

/// Staleness reason codes
typedef enum pc_staleness_reason {
  PC_STALE_NONE = 0,
  PC_STALE_PAGE_COUNT = 1,     // page count mismatch (R5.1)
  PC_STALE_FINGERPRINT = 2,    // document fingerprint changed (R5.2 strong signal)
  PC_STALE_MODIFIED_TIME = 3,  // modification time changed (R5.2 weak signal)
} pc_staleness_reason;

struct pc_sidecar {
  int format_version;
  int read_only;          // set when format_version > PC_SIDECAR_FORMAT_VERSION_SUPPORTED (R2.2)
  char* document_sha256;  // stored fingerprint
  char* document_path;
  int64_t modified_time;  // stored modification time
  int page_count;         // stored page count
  char* annotations_json;
  char* unknown_json;
  int is_stale;                      // 1 if stale detected (R5.1, R5.2)
  pc_staleness_reason stale_reason;  // why it is stale
  char* stale_detail;                // owned; formatted report when stale
};

/// Write sidecar atomically (R3.1 tmp->rename + fsync, R3.2 lock, R4.1 unknown preserve, R6
/// exclusions). Refuses a stale sidecar (R5.1: returns PC_ERR_STATE) and a sidecar whose
/// format_version exceeds PC_SIDECAR_FORMAT_VERSION_SUPPORTED (R2.2: read-only, PC_ERR_STATE).
/// Returns PC_ERR_NONE on success, PC_ERR_ARGUMENT if path mismatch (R4.2),
/// PC_ERR_STATE if locked, stale, or read-only
pc_status pc_sidecar_write(const char* doc_path, const pc_sidecar* sidecar);

/// Write like pc_sidecar_write, but an explicit force bypasses the stale guard only
/// (R5.1: "writes disabled until the caller passes force=true"). The read-only guard
/// (future format_version) is absolute and cannot be forced.
pc_status pc_sidecar_write_force(const char* doc_path, const pc_sidecar* sidecar);

/// Read sidecar; populates is_stale/stale_reason/stale_detail by checking current document
/// (R5.1, R5.2). When format_version > PC_SIDECAR_FORMAT_VERSION_SUPPORTED, returns
/// PC_ERR_VERSION with detail "format_version X > supported Y" and *out_sidecar set to a
/// valid read-only view (R2.2). Returns PC_ERR_NONE on success, PC_ERR_IO if not found.
/// Caller owns *out_sidecar, must free with pc_sidecar_free
pc_status pc_sidecar_read(const char* doc_path, pc_sidecar** out_sidecar);

/// Free sidecar allocated by pc_sidecar_read
void pc_sidecar_free(pc_sidecar* sidecar);

/// Acquire .tynypdf.lock for 5-minute exclusive write window (R3.2)
/// Returns PC_ERR_NONE if acquired, PC_ERR_STATE if lock exists and is fresh
pc_status pc_sidecar_try_lock(const char* doc_path);

/// Release .tynypdf.lock
void pc_sidecar_unlock(const char* doc_path);

/// Validate an annotation or reply ID per R2.3: ^[a-z2-7]{10}$ (RFC 4648 base32 lowercase,
/// no padding). Returns PC_ERR_NONE if valid, PC_ERR_ARGUMENT if invalid.
pc_status pc_sidecar_validate_annotation_id(const char* annotation_id);

/// Check staleness of sidecar against current document (R5.1, R5.2).
/// Sets is_stale, stale_reason and stale_detail. Returns PC_ERR_STATE (with stale_detail as
/// the message) when stale, PC_ERR_NONE when fresh.
pc_status pc_sidecar_check_staleness(const char* doc_path, pc_sidecar* sidecar);

/// 1 if the sidecar was found stale by a previous read/check, 0 otherwise (R5.1)
int pc_sidecar_is_stale(const pc_sidecar* sidecar);

/// Copy the frozen stale report (stale_detail) into buf. Empty when not stale.
/// Returns PC_ERR_NONE on success.
pc_status pc_sidecar_stale_report(const pc_sidecar* sidecar, char* buf, size_t cap);

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