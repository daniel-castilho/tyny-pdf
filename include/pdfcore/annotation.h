#ifndef PDFCORE_ANNOTATION_H
#define PDFCORE_ANNOTATION_H

#include <stddef.h>
#include <stdint.h>

#include "status.h"
#include "geom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_annotation pc_annotation;

/// Annotation types
typedef enum pc_annotation_type {
  PC_ANNOT_NONE = 0,
  PC_ANNOT_TEXT,        // Text annotation (sticky note)
  PC_ANNOT_HIGHLIGHT,   // Text highlight
  PC_ANNOT_UNDERLINE,   // Text underline
  PC_ANNOT_STRIKETHROUGH, // Text strikethrough
  PC_ANNOT_SQUIGGLY,    // Squiggly underline
  PC_ANNOT_FREE_TEXT,   // Free text annotation
  PC_ANNOT_LINE,        // Line annotation
  PC_ANNOT_SQUARE,      // Square/rectangle
  PC_ANNOT_CIRCLE,      // Circle/ellipse
  PC_ANNOT_POLYGON,     // Polygon
  PC_ANNOT_POLYLINE,    // Polyline
  PC_ANNOT_INK,         // Ink (freehand)
  PC_ANNOT_STAMP,       // Rubber stamp
  PC_ANNOT_CARET,       // Caret (text insertion point)
  PC_ANNOT_LINK,        // Link annotation
  PC_ANNOT_WIDGET,      // Form field widget
  PC_ANNOT_REDACTION,   // Redaction annotation
} pc_annotation_type;

/// Annotation flags
typedef enum pc_annotation_flag {
  PC_ANNOT_FLAG_INVISIBLE = 1 << 0,
  PC_ANNOT_FLAG_HIDDEN = 1 << 1,
  PC_ANNOT_FLAG_PRINT = 1 << 2,
  PC_ANNOT_FLAG_NO_ZOOM = 1 << 2,
  PC_ANNOT_FLAG_NO_ROTATE = 1 << 3,
  PC_ANNOT_FLAG_NO_VIEW = 1 << 4,
  PC_ANNOT_FLAG_READ_ONLY = 1 << 5,
  PC_ANNOT_FLAG_LOCKED = 1 << 6,
  PC_ANNOT_FLAG_TOGGLE_NO_VIEW = 1 << 7,
  PC_ANNOT_FLAG_LOCKED_CONTENTS = 1 << 8,
} pc_annotation_flag;

/// Annotation record in the IR
struct pc_annotation {
  char* id;                    // 10-char RFC 4648 base32 ID (R2.3)
  pc_annotation_type type;
  uint32_t page_index;         // Page this annotation belongs to
  pc_rect rect;                // Position and size in user space
  char* contents;              // Text contents (for text annotations)
  char* author;                // Author name
  char* creation_date;         // Creation date (ISO 8601)
  char* modification_date;     // Modification date (ISO 8601)
  uint32_t flags;              // Bitmask of pc_annotation_flag
  float color[3];              // RGB color (0.0-1.0)
  float opacity;               // Opacity (0.0-1.0)
  float border_width;          // Border width in points
  float* quad_points;          // Quad points for text markup (4 points per quad)
  size_t quad_count;           // Number of quad points
  char* custom_data;           // JSON for type-specific data
};

/// Annotation list in the IR
typedef struct pc_annotation_list {
  pc_annotation** annotations;
  size_t count;
  size_t capacity;
} pc_annotation_list;

/// Create empty annotation list
pc_annotation_list* pc_annotation_list_create(void);

/// Free annotation list and all annotations
void pc_annotation_list_free(pc_annotation_list* list);

/// Add annotation to list
/// Takes ownership of annotation
pc_status pc_annotation_list_add(pc_annotation_list* list, pc_annotation* annot);

/// Remove annotation by ID
/// Returns PC_ERR_NONE on success, PC_ERR_NOT_FOUND if not found
pc_status pc_annotation_list_remove(pc_annotation_list* list, const char* id);

/// Find annotation by ID
pc_annotation* pc_annotation_list_find(pc_annotation_list* list, const char* id);

/// Get annotations for a specific page
size_t pc_annotation_list_count_for_page(pc_annotation_list* list, uint32_t page_index);

/// Get annotations for a specific page (fills provided array)
void pc_annotation_list_get_for_page(pc_annotation_list* list, uint32_t page_index, pc_annotation** out, size_t max_count);

/// Create annotation with auto-generated ID (R2.3 compliant)
/// Returns allocated annotation, must be freed with pc_annotation_free
pc_annotation* pc_annotation_create(pc_annotation_type type, uint32_t page_index, const pc_rect* rect);

/// Free annotation
void pc_annotation_free(pc_annotation* annot);

/// Serialize annotation to JSON (for transaction log)
char* pc_annotation_to_json(const pc_annotation* annot);

/// Deserialize annotation from JSON
pc_annotation* pc_annotation_from_json(const char* json);

/// Serialize annotation list to JSON
char* pc_annotation_list_to_json(const pc_annotation_list* list);

/// Deserialize annotation list from JSON
pc_annotation_list* pc_annotation_list_from_json(const char* json);

#ifdef __cplusplus
}
#endif

#endif