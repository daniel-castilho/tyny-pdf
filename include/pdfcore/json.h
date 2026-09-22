#ifndef PDFCORE_JSON_H
#define PDFCORE_JSON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Canonical JSON (ADR-0007): keys sorted, 2-space indent, LF, 3 decimal places for
/// floats, empty object "{}", empty array "[]", no trailing commas or whitespace other
/// than the final LF.

typedef enum pc_json_type {
  PC_JSON_NULL = 0,
  PC_JSON_BOOL = 1,
  PC_JSON_INT = 2,
  PC_JSON_DOUBLE = 3,
  PC_JSON_STRING = 4,
  PC_JSON_OBJECT = 5,
  PC_JSON_ARRAY = 6,
} pc_json_type;

typedef struct pc_json_value pc_json_value;

struct pc_json_pair {
  const char* key;
  pc_json_value* value;
};

pc_json_value* pc_json_string(const char* s);
pc_json_value* pc_json_int(int64_t v);
pc_json_value* pc_json_double(double v);
pc_json_value* pc_json_bool(int v);
pc_json_value* pc_json_null(void);

/// `pairs` is terminated by a pair whose key is NULL. The object takes ownership of values.
pc_json_value* pc_json_object(const struct pc_json_pair* pairs);

/// `values` is terminated by NULL. The array takes ownership of values.
pc_json_value* pc_json_array(pc_json_value* const* values);

int pc_json_serialize(const pc_json_value* root, char* buf, size_t cap);
void pc_json_free(pc_json_value* v);
pc_json_value* pc_json_parse(const char* json);
pc_json_value* pc_json_clone(const pc_json_value* v);

pc_json_type pc_json_get_type(const pc_json_value* v);
const char* pc_json_as_string(const pc_json_value* v);
int64_t pc_json_as_int(const pc_json_value* v);
double pc_json_as_number(const pc_json_value* v);
int pc_json_as_bool(const pc_json_value* v);

const pc_json_value* pc_json_object_get(const pc_json_value* obj, const char* key);
void pc_json_object_put(pc_json_value* obj, const char* key, pc_json_value* value);
size_t pc_json_object_size(const pc_json_value* obj);
const char* pc_json_object_key_at(const pc_json_value* obj, size_t index);
const pc_json_value* pc_json_object_value_at(const pc_json_value* obj, size_t index);

size_t pc_json_array_size(const pc_json_value* arr);
const pc_json_value* pc_json_array_at(const pc_json_value* arr, size_t index);

#ifdef __cplusplus
}
#endif

#endif
