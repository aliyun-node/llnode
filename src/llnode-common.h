#ifndef SRC_COMMON_H
#define SRC_COMMON_H

#include <string>

namespace llnode {
enum InspectType {
  kNoObjectSmi = 1,
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

enum FrameType {
  kNativeFrame = 1,
  kJsFrame
};

typedef struct {
  InspectType type;
  std::string name;
  std::string address;
  std::string map_address;
} inspect_t;

typedef struct {
  FrameType type;
  std::string name;
  std::string function;
} frame_t;

typedef struct Args {
  int length;
  inspect_t* context = nullptr;
  inspect_t** args_list = nullptr;
  ~Args() {
    this->length = 0;
    delete this->context;
    this->context = nullptr;
    delete[] this->args_list;
    this->args_list = nullptr;
  }
} args_t;

typedef struct JSFunctionDebug {
  std::string func_name;
  std::string line;
  ~JSFunctionDebug() {
    this->func_name = "";
    this->line = "";
  }
} js_function_debug_t;

typedef struct: frame_t {
  std::string module_file;
  std::string compile_unit_file;
} native_frame_t;

typedef struct JSFrame: frame_t {
  args_t* args = nullptr;
  js_function_debug_t* debug = nullptr;
  std::string address;
  ~JSFrame() {
    delete this->args;
    this->args = nullptr;
    delete this->debug;
    this->debug = nullptr;
    this->address = "";
  }
} js_frame_t;

typedef struct {
  std::string key;
  inspect_t* value = nullptr;
  std::string value_str;
} property_t;

typedef struct {
  int length;
  property_t** properties = nullptr;
} properties_t;

typedef struct {
  int length;
  inspect_t** elements = nullptr;
} elements_t;

typedef struct {
  std::string address;
} internal_filed_t;

typedef struct {
  int length;
  internal_filed_t** internal_fileds = nullptr;
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
  inspect_t* descriptors_array = nullptr;
} map_t;

typedef struct: inspect_t {
  int length;
  inspect_t** content = nullptr;
} fixed_array_t;

typedef struct: inspect_t {
  std::string constructor;
  elements_t* elements = nullptr;
  properties_t* properties = nullptr;
  internal_fileds_t* fields = nullptr;
} js_object_t;

typedef struct: inspect_t {
  std::string value;
} heap_number_t;

typedef struct: inspect_t {
  int total_length;
  elements_t* display_elemets = nullptr;
} js_array_t;

typedef struct: inspect_t {
  std::string value;
} oddball_t;

typedef struct: inspect_t {
  std::string func_name;
  std::string func_source;
  std::string debug_line;
  std::string context_address;
  inspect_t* context = nullptr;
} js_function_t;

typedef struct: inspect_t {
  std::string previous_address;
  std::string closure_address;
  inspect_t* closure = nullptr;
  properties_t* scope_object = nullptr;
} context_t;

typedef struct: inspect_t {
  std::string source;
  elements_t* elements = nullptr;
  properties_t* properties = nullptr;
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
  int display_length;
  std::string* elements = nullptr;
} js_array_buffer_t;

typedef struct: inspect_t {
  // if true, show "[neutered]"
  bool neutered;
  int byte_length;
  int byte_offset;
  std::string backing_store_address;
  int display_length;
  std::string* elements = nullptr;
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
