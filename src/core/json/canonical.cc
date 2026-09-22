#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "pdfcore/json.h"

struct pc_json_value {
  pc_json_type type = PC_JSON_NULL;
  int64_t int_val = 0;
  double double_val = 0.0;
  int bool_val = 0;
  char* str_val = nullptr;
  std::map<std::string, pc_json_value*> obj_val;
  std::vector<pc_json_value*> arr_val;
};

static pc_json_value* new_value(pc_json_type t) {
  pc_json_value* v = new (std::nothrow) pc_json_value();
  if (v) {
    v->type = t;
  }
  return v;
}

pc_json_value* pc_json_string(const char* s) {
  pc_json_value* v = new_value(PC_JSON_STRING);
  if (!v) {
    return nullptr;
  }
  v->str_val = strdup(s ? s : "");
  if (!v->str_val) {
    delete v;
    return nullptr;
  }
  return v;
}

pc_json_value* pc_json_int(int64_t v) {
  pc_json_value* n = new_value(PC_JSON_INT);
  if (n) {
    n->int_val = v;
  }
  return n;
}

pc_json_value* pc_json_double(double v) {
  pc_json_value* n = new_value(PC_JSON_DOUBLE);
  if (n) {
    n->double_val = v;
  }
  return n;
}

pc_json_value* pc_json_bool(int v) {
  pc_json_value* n = new_value(PC_JSON_BOOL);
  if (n) {
    n->bool_val = v ? 1 : 0;
  }
  return n;
}

pc_json_value* pc_json_null(void) {
  return new_value(PC_JSON_NULL);
}

pc_json_value* pc_json_object(const struct pc_json_pair* pairs) {
  pc_json_value* v = new_value(PC_JSON_OBJECT);
  if (!v) {
    return nullptr;
  }
  if (pairs) {
    for (int i = 0; pairs[i].key; ++i) {
      v->obj_val[pairs[i].key] = pairs[i].value;
    }
  }
  return v;
}

pc_json_value* pc_json_array(pc_json_value* const* values) {
  pc_json_value* v = new_value(PC_JSON_ARRAY);
  if (!v) {
    return nullptr;
  }
  if (values) {
    for (int i = 0; values[i]; ++i) {
      v->arr_val.push_back(values[i]);
    }
  }
  return v;
}

static int json_append(char* buf, size_t* pos, size_t cap, const char* s) {
  size_t len = strlen(s);
  if (*pos + len >= cap) {
    return -1;
  }
  memcpy(buf + *pos, s, len);
  *pos += len;
  return 0;
}

static int json_append_int(char* buf, size_t* pos, size_t cap, int64_t v) {
  char tmp[32];
  int n = snprintf(tmp, sizeof(tmp), "%lld", (long long)v);
  if (n < 0) {
    return -1;
  }
  return json_append(buf, pos, cap, tmp);
}

static int json_append_double(char* buf, size_t* pos, size_t cap, double v) {
  char tmp[32];
  int n = snprintf(tmp, sizeof(tmp), "%.3f", v);
  if (n < 0) {
    return -1;
  }
  return json_append(buf, pos, cap, tmp);
}

static int json_append_escaped(char* buf, size_t* pos, size_t cap, const char* s) {
  if (!s) {
    s = "";
  }
  if (json_append(buf, pos, cap, "\"") < 0) {
    return -1;
  }
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
    char esc[8];
    const char* out = nullptr;
    switch (*p) {
      case '"':
        out = "\\\"";
        break;
      case '\\':
        out = "\\\\";
        break;
      case '\b':
        out = "\\b";
        break;
      case '\f':
        out = "\\f";
        break;
      case '\n':
        out = "\\n";
        break;
      case '\r':
        out = "\\r";
        break;
      case '\t':
        out = "\\t";
        break;
      default:
        if (*p < 0x20) {
          snprintf(esc, sizeof(esc), "\\u%04x", *p);
          out = esc;
        }
        break;
    }
    if (out) {
      if (json_append(buf, pos, cap, out) < 0) {
        return -1;
      }
    } else {
      if (*pos + 1 >= cap) {
        return -1;
      }
      buf[(*pos)++] = static_cast<char>(*p);
    }
  }
  return json_append(buf, pos, cap, "\"");
}

static int serialize_value(const pc_json_value* v, char* buf, size_t* pos, size_t cap, int indent);

static int serialize_object(const pc_json_value* v, char* buf, size_t* pos, size_t cap,
                            int indent) {
  if (v->obj_val.empty()) {
    return json_append(buf, pos, cap, "{}");
  }
  if (json_append(buf, pos, cap, "{\n") < 0) {
    return -1;
  }
  size_t n = v->obj_val.size();
  size_t i = 0;
  for (const auto& kv : v->obj_val) {
    for (int d = 0; d < indent + 1; ++d) {
      if (json_append(buf, pos, cap, "  ") < 0) {
        return -1;
      }
    }
    if (json_append_escaped(buf, pos, cap, kv.first.c_str()) < 0) {
      return -1;
    }
    if (json_append(buf, pos, cap, ": ") < 0) {
      return -1;
    }
    if (serialize_value(kv.second, buf, pos, cap, indent + 1) < 0) {
      return -1;
    }
    if (i + 1 < n) {
      if (json_append(buf, pos, cap, ",\n") < 0) {
        return -1;
      }
    } else {
      if (json_append(buf, pos, cap, "\n") < 0) {
        return -1;
      }
    }
    ++i;
  }
  for (int d = 0; d < indent; ++d) {
    if (json_append(buf, pos, cap, "  ") < 0) {
      return -1;
    }
  }
  return json_append(buf, pos, cap, "}");
}

static int serialize_array(const pc_json_value* v, char* buf, size_t* pos, size_t cap, int indent) {
  if (v->arr_val.empty()) {
    return json_append(buf, pos, cap, "[]");
  }
  if (json_append(buf, pos, cap, "[\n") < 0) {
    return -1;
  }
  for (size_t i = 0; i < v->arr_val.size(); ++i) {
    for (int d = 0; d < indent + 1; ++d) {
      if (json_append(buf, pos, cap, "  ") < 0) {
        return -1;
      }
    }
    if (serialize_value(v->arr_val[i], buf, pos, cap, indent + 1) < 0) {
      return -1;
    }
    if (i + 1 < v->arr_val.size()) {
      if (json_append(buf, pos, cap, ",\n") < 0) {
        return -1;
      }
    } else {
      if (json_append(buf, pos, cap, "\n") < 0) {
        return -1;
      }
    }
  }
  for (int d = 0; d < indent; ++d) {
    if (json_append(buf, pos, cap, "  ") < 0) {
      return -1;
    }
  }
  return json_append(buf, pos, cap, "]");
}

static int serialize_value(const pc_json_value* v, char* buf, size_t* pos, size_t cap, int indent) {
  if (!v) {
    return -1;
  }
  switch (v->type) {
    case PC_JSON_NULL:
      return json_append(buf, pos, cap, "null");
    case PC_JSON_BOOL:
      return json_append(buf, pos, cap, v->bool_val ? "true" : "false");
    case PC_JSON_INT:
      return json_append_int(buf, pos, cap, v->int_val);
    case PC_JSON_DOUBLE:
      return json_append_double(buf, pos, cap, v->double_val);
    case PC_JSON_STRING:
      return json_append_escaped(buf, pos, cap, v->str_val);
    case PC_JSON_OBJECT:
      return serialize_object(v, buf, pos, cap, indent);
    case PC_JSON_ARRAY:
      return serialize_array(v, buf, pos, cap, indent);
    default:
      return -1;
  }
}

int pc_json_serialize(const pc_json_value* root, char* buf, size_t cap) {
  if (!root || !buf || cap < 4) {
    return -1;
  }
  size_t pos = 0;
  if (serialize_value(root, buf, &pos, cap, 0) < 0) {
    return -1;
  }
  if (json_append(buf, &pos, cap, "\n") < 0) {
    return -1;
  }
  buf[pos] = '\0';
  return static_cast<int>(pos);
}

void pc_json_free(pc_json_value* v) {
  if (!v) {
    return;
  }
  switch (v->type) {
    case PC_JSON_STRING:
      free(v->str_val);
      break;
    case PC_JSON_OBJECT:
      for (auto& kv : v->obj_val) {
        pc_json_free(kv.second);
      }
      break;
    case PC_JSON_ARRAY:
      for (pc_json_value* c : v->arr_val) {
        pc_json_free(c);
      }
      break;
    default:
      break;
  }
  delete v;
}

pc_json_value* pc_json_clone(const pc_json_value* v) {
  if (!v) {
    return nullptr;
  }
  switch (v->type) {
    case PC_JSON_NULL:
      return pc_json_null();
    case PC_JSON_BOOL:
      return pc_json_bool(v->bool_val);
    case PC_JSON_INT:
      return pc_json_int(v->int_val);
    case PC_JSON_DOUBLE:
      return pc_json_double(v->double_val);
    case PC_JSON_STRING:
      return pc_json_string(v->str_val);
    case PC_JSON_OBJECT: {
      pc_json_value* o = new_value(PC_JSON_OBJECT);
      if (!o) {
        return nullptr;
      }
      for (const auto& kv : v->obj_val) {
        o->obj_val[kv.first] = pc_json_clone(kv.second);
      }
      return o;
    }
    case PC_JSON_ARRAY: {
      pc_json_value* a = new_value(PC_JSON_ARRAY);
      if (!a) {
        return nullptr;
      }
      for (pc_json_value* c : v->arr_val) {
        a->arr_val.push_back(pc_json_clone(c));
      }
      return a;
    }
    default:
      return nullptr;
  }
}

pc_json_type pc_json_get_type(const pc_json_value* v) {
  return v ? v->type : PC_JSON_NULL;
}

const char* pc_json_as_string(const pc_json_value* v) {
  return (v && v->type == PC_JSON_STRING) ? v->str_val : nullptr;
}

int64_t pc_json_as_int(const pc_json_value* v) {
  if (!v) {
    return 0;
  }
  if (v->type == PC_JSON_INT) {
    return v->int_val;
  }
  if (v->type == PC_JSON_DOUBLE) {
    return static_cast<int64_t>(v->double_val);
  }
  return 0;
}

double pc_json_as_number(const pc_json_value* v) {
  if (!v) {
    return 0.0;
  }
  if (v->type == PC_JSON_DOUBLE) {
    return v->double_val;
  }
  if (v->type == PC_JSON_INT) {
    return static_cast<double>(v->int_val);
  }
  return 0.0;
}

int pc_json_as_bool(const pc_json_value* v) {
  return (v && v->type == PC_JSON_BOOL) ? v->bool_val : 0;
}

const pc_json_value* pc_json_object_get(const pc_json_value* obj, const char* key) {
  if (!obj || obj->type != PC_JSON_OBJECT || !key) {
    return nullptr;
  }
  auto it = obj->obj_val.find(key);
  return (it == obj->obj_val.end()) ? nullptr : it->second;
}

void pc_json_object_put(pc_json_value* obj, const char* key, pc_json_value* value) {
  if (!obj || obj->type != PC_JSON_OBJECT || !key) {
    pc_json_free(value);
    return;
  }
  auto it = obj->obj_val.find(key);
  if (it != obj->obj_val.end()) {
    pc_json_free(it->second);
  }
  obj->obj_val[key] = value;
}

size_t pc_json_object_size(const pc_json_value* obj) {
  return (obj && obj->type == PC_JSON_OBJECT) ? obj->obj_val.size() : 0;
}

const char* pc_json_object_key_at(const pc_json_value* obj, size_t index) {
  if (!obj || obj->type != PC_JSON_OBJECT) {
    return nullptr;
  }
  size_t i = 0;
  for (const auto& kv : obj->obj_val) {
    if (i == index) {
      return kv.first.c_str();
    }
    ++i;
  }
  return nullptr;
}

const pc_json_value* pc_json_object_value_at(const pc_json_value* obj, size_t index) {
  if (!obj || obj->type != PC_JSON_OBJECT) {
    return nullptr;
  }
  size_t i = 0;
  for (const auto& kv : obj->obj_val) {
    if (i == index) {
      return kv.second;
    }
    ++i;
  }
  return nullptr;
}

size_t pc_json_array_size(const pc_json_value* arr) {
  return (arr && arr->type == PC_JSON_ARRAY) ? arr->arr_val.size() : 0;
}

const pc_json_value* pc_json_array_at(const pc_json_value* arr, size_t index) {
  if (!arr || arr->type != PC_JSON_ARRAY || index >= arr->arr_val.size()) {
    return nullptr;
  }
  return arr->arr_val[index];
}

static void skip_ws(const char** s) {
  while (**s == ' ' || **s == '\t' || **s == '\n' || **s == '\r') {
    (*s)++;
  }
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static pc_json_value* parse_json_value(const char** s);

static pc_json_value* parse_json_string(const char** s) {
  if (**s != '"') {
    return nullptr;
  }
  (*s)++;
  std::string out;
  while (**s && **s != '"') {
    if (**s == '\\') {
      (*s)++;
      char esc = **s;
      if (!esc) {
        return nullptr;
      }
      switch (esc) {
        case '"':
        case '\\':
        case '/':
          out.push_back(esc);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u': {
          unsigned code = 0;
          for (int i = 0; i < 4; ++i) {
            (*s)++;
            int n = hex_nibble(**s);
            if (n < 0) {
              return nullptr;
            }
            code = (code << 4) | static_cast<unsigned>(n);
          }
          if (code < 0x80) {
            out.push_back(static_cast<char>(code));
          } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          } else {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
          }
          break;
        }
        default:
          return nullptr;
      }
      (*s)++;
    } else {
      out.push_back(**s);
      (*s)++;
    }
  }
  if (**s != '"') {
    return nullptr;
  }
  (*s)++;
  return pc_json_string(out.c_str());
}

static pc_json_value* parse_json_number(const char** s) {
  const char* p = *s;
  int neg = 0;
  if (*p == '-') {
    neg = 1;
    p++;
  }
  if (*p < '0' || *p > '9') {
    return nullptr;
  }
  int64_t ival = 0;
  while (*p >= '0' && *p <= '9') {
    ival = ival * 10 + (*p - '0');
    p++;
  }
  if (*p == '.') {
    p++;
    double fval = static_cast<double>(ival);
    double frac = 0.1;
    if (*p < '0' || *p > '9') {
      return nullptr;
    }
    while (*p >= '0' && *p <= '9') {
      fval += (*p - '0') * frac;
      frac *= 0.1;
      p++;
    }
    if (neg) {
      fval = -fval;
    }
    *s = p;
    return pc_json_double(fval);
  }
  if (neg) {
    ival = -ival;
  }
  *s = p;
  return pc_json_int(ival);
}

static pc_json_value* parse_json_object(const char** s) {
  if (**s != '{') {
    return nullptr;
  }
  (*s)++;
  skip_ws(s);
  pc_json_value* v = new_value(PC_JSON_OBJECT);
  if (!v) {
    return nullptr;
  }
  if (**s == '}') {
    (*s)++;
    return v;
  }
  while (**s) {
    skip_ws(s);
    pc_json_value* key = parse_json_string(s);
    if (!key) {
      pc_json_free(v);
      return nullptr;
    }
    skip_ws(s);
    if (**s != ':') {
      pc_json_free(v);
      pc_json_free(key);
      return nullptr;
    }
    (*s)++;
    skip_ws(s);
    pc_json_value* val = parse_json_value(s);
    if (!val) {
      pc_json_free(v);
      pc_json_free(key);
      return nullptr;
    }
    v->obj_val[key->str_val] = val;
    pc_json_free(key);
    skip_ws(s);
    if (**s == '}') {
      (*s)++;
      break;
    }
    if (**s != ',') {
      pc_json_free(v);
      return nullptr;
    }
    (*s)++;
  }
  return v;
}

static pc_json_value* parse_json_array(const char** s) {
  if (**s != '[') {
    return nullptr;
  }
  (*s)++;
  skip_ws(s);
  pc_json_value* v = new_value(PC_JSON_ARRAY);
  if (!v) {
    return nullptr;
  }
  if (**s == ']') {
    (*s)++;
    return v;
  }
  while (**s) {
    skip_ws(s);
    pc_json_value* val = parse_json_value(s);
    if (!val) {
      pc_json_free(v);
      return nullptr;
    }
    v->arr_val.push_back(val);
    skip_ws(s);
    if (**s == ']') {
      (*s)++;
      break;
    }
    if (**s != ',') {
      pc_json_free(v);
      return nullptr;
    }
    (*s)++;
  }
  return v;
}

static pc_json_value* parse_json_value(const char** s) {
  skip_ws(s);
  if (**s == '"') {
    return parse_json_string(s);
  }
  if (**s == '{') {
    return parse_json_object(s);
  }
  if (**s == '[') {
    return parse_json_array(s);
  }
  if (strncmp(*s, "true", 4) == 0) {
    *s += 4;
    return pc_json_bool(1);
  }
  if (strncmp(*s, "false", 5) == 0) {
    *s += 5;
    return pc_json_bool(0);
  }
  if (strncmp(*s, "null", 4) == 0) {
    *s += 4;
    return pc_json_null();
  }
  if (**s == '-' || (**s >= '0' && **s <= '9')) {
    return parse_json_number(s);
  }
  return nullptr;
}

pc_json_value* pc_json_parse(const char* json) {
  if (!json) {
    return nullptr;
  }
  const char* p = json;
  pc_json_value* v = parse_json_value(&p);
  if (!v) {
    return nullptr;
  }
  skip_ws(&p);
  if (*p != '\0') {
    pc_json_free(v);
    return nullptr;
  }
  return v;
}
