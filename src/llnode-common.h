#ifndef SRC_COMMON_H
#define SRC_COMMON_H

#include <string>

namespace llnode {
typedef struct {
  std::string symbol;
  std::string function;
  std::string module_file;
  std::string compile_unit_file;
} native_frame_t;

typedef struct {
  std::string function;
  std::string context;
  std::string arguments;
  std::string line;
  std::string address;
} valid_js_frame_t;

typedef struct {
  int type;
  std::string symbol;
  union {
    const char* invalid_js_frame;
    valid_js_frame_t* valid_js_frame;
  };
} js_frame_t;

typedef struct {
  int type;
  union {
    native_frame_t* native_frame;
    js_frame_t* js_frame;
  };
} frame_t;

enum InspectType {
  kNoObjectSmi,
  kSmi,
  kGlobalObject,
  kGlobalProxy,
  kCode,
  kMap,
  kFixedArray,
  kJsObject,
  kHeapNumber,
  kJsArray,
  kOddball,
  kJsFunction,
  kJsRegExp,
  kFirstNonstring,
  kJsArrayBuffer,
  kJsArrayBufferView,
  kJsDate,
  kUnknown
};

typedef struct {
  InspectType type;
  std::string name;
  std::string address;
  std::string map_address;
} inspect_t;


typedef struct {
  std::string key;
  inspect_t* value;
  std::string value_str;
} property_t;

typedef struct {
  int length;
  property_t** properties;
} properties_t;

typedef struct {
  int length;
  inspect_t** elements;
} elements_t;

typedef struct {
  std::string address;
} internal_filed_t;

typedef struct {
  int length;
  internal_filed_t** internal_fileds;
} internal_fileds_t;


typedef struct: inspect_t {
  std::string value;
} smi_t;

typedef struct: inspect_t {
  int own_descriptors;
  std::string in_object_properties_or_constructor;
  int in_object_properties_or_constructor_index;
  int instance_size;
  std::string descriptors_address;
  inspect_t* descriptors_array;
} map_t;

typedef struct: inspect_t {
  int length;
  inspect_t** content;
} fixed_array_t;

typedef struct: inspect_t {
  std::string constructor;
  elements_t* elements;
  properties_t* properties;
  internal_fileds_t* fields;
} js_object_t;

typedef struct: inspect_t {
  std::string value;
} heap_number_t;

typedef struct: inspect_t {
  int total_length;
  elements_t* display_elemets;
} js_array_t;

typedef struct: inspect_t {
  std::string value;
} odd_ball_t;

typedef struct: inspect_t {
  std::string debug_line;
  std::string context_address;
  inspect_t* context;
  std::string func_name;
  std::string func_source;
} js_function_t;

typedef struct: inspect_t {
  std::string previous_address;
  std::string closure_address;
  inspect_t* closure;
  properties_t* scope_object;
} context_t;

typedef struct: inspect_t {
  std::string source;
  elements_t* elements;
  properties_t* properties;
} js_regexp_t;

typedef struct: inspect_t {
  int total_length;
  std::string display_value;
} first_non_string_t;

typedef struct: inspect_t {
  // if true, show "[neutered]"
  bool neutered;
  int byte_length;
  std::string backing_store_address;
  std::string* elements;
} js_array_buffer_t;

typedef struct: inspect_t {
  // if true, show "[neutered]"
  bool neutered;
  int byte_length;
  int byte_offset;
  std::string backing_store_address;
  std::string* elements;
} js_array_buffer_view_t;

typedef struct: inspect_t {
  std::string value;
} js_date_t;

class LLMonitor {
public:
  explicit LLMonitor();
  ~LLMonitor();
  void SetProgress(double progress);
  double GetProgress();
private:
  double progress;
};
}

#endif
