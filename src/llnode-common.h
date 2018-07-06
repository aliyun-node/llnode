#ifndef SRC_COMMON_H
#define SRC_COMMON_H

#include <string>

namespace llnode {
enum InspectType {
  kUninitializedInspect,
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
  kContext,
  kUnknown
};

enum FrameType {
  kUninitializedFrame,
  kNativeFrame,
  kJsFrame
};

typedef struct Inspect {
  InspectType type;
  std::string name = "";
  std::string address = "";
  std::string map_address = "";
  ~Inspect() {
    this->type = kUninitializedInspect;
    this->name = "";
    this->address = "";
    this->map_address = "";
  }
} inspect_t;

typedef struct Frame {
  FrameType type;
  std::string name = "";
  std::string function = "";
  ~Frame() {
    this->type = kUninitializedFrame;
    this->name = "";
    this->function = "";
  }
} frame_t;

typedef struct Args {
  int length = 0;
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
  std::string func_name = "";
  std::string line = "";
  ~JSFunctionDebug() {
    this->func_name = "";
    this->line = "";
  }
} js_function_debug_t;

typedef struct NativeFrame: frame_t {
  std::string module_file = "";
  std::string compile_unit_file = "";
  ~NativeFrame() {
    this->module_file = "";
    this->compile_unit_file = "";
  }
} native_frame_t;

typedef struct JSFrame: frame_t {
  args_t* args = nullptr;
  js_function_debug_t* debug = nullptr;
  std::string address = "";
  ~JSFrame() {
    delete this->args;
    this->args = nullptr;
    delete this->debug;
    this->debug = nullptr;
    this->address = "";
  }
} js_frame_t;

typedef struct Property {
  std::string key = "";
  inspect_t* value = nullptr;
  std::string value_str = "";
  ~Property() {
    this->key = "";
    delete this->value;
    this->value = nullptr;
    this->value_str = "";
  }
} property_t;

typedef struct Properties {
  int length = 0;
  property_t** properties = nullptr;
  ~Properties() {
    this->length = 0;
    delete[] this->properties;
    this->properties = nullptr;
  }
} properties_t;

typedef struct Element {
  int length = 0;
  inspect_t** elements = nullptr;
  ~Element() {
    this->length = 0;
    delete[] this->elements;
    this->elements = nullptr;
  }
} elements_t;

typedef struct InternalField {
  std::string address = "";
  ~InternalField() {
    this->address = "";
  }
} internal_filed_t;

typedef struct InternalFields {
  int length = 0;
  internal_filed_t** internal_fileds = nullptr;
  ~InternalFields() {
    this->length = 0;
    delete[] this->internal_fileds;
    this->internal_fileds = nullptr;
  }
} internal_fileds_t;

typedef struct Smi: inspect_t {
  std::string value = "";
  ~Smi() {
    this->value = "";
  }
} smi_t;

typedef struct Map: inspect_t {
  int own_descriptors = 0;
  std::string in_object_properties_or_constructor = "";
  int in_object_properties_or_constructor_index = 0;
  int instance_size = 0;
  std::string descriptors_address = "";
  inspect_t* descriptors_array = nullptr;
  ~Map() {
    this->own_descriptors = 0;
    this->in_object_properties_or_constructor = "";
    this->in_object_properties_or_constructor_index = 0;
    this->instance_size = 0;
    this->descriptors_address = "";
    delete this->descriptors_array;
    this->descriptors_array = nullptr;
  }
} map_t;

typedef struct FixedArray: elements_t, inspect_t {
} fixed_array_t;

typedef struct JsObject: inspect_t {
  std::string constructor = "";
  int64_t elements_length = 0;
  int64_t properties_length = 0;
  int64_t fields_length = 0;
  elements_t* elements = nullptr;
  properties_t* properties = nullptr;
  internal_fileds_t* fields = nullptr;
  ~JsObject() {
    this->constructor = "";
    this->elements_length = 0;
    this->properties_length = 0;
    this->fields_length = 0;
    delete this->elements;
    this->elements = nullptr;
    delete this->properties;
    this->properties = nullptr;
    delete this->fields;
    this->fields = nullptr;
  }
} js_object_t;

typedef struct HeapNumber: inspect_t {
  std::string value = "";
  ~HeapNumber() {
    this->value = "";
  }
} heap_number_t;

typedef struct JsArray: inspect_t {
  int total_length = 0;
  elements_t* display_elemets = nullptr;
  ~JsArray() {
    this->total_length = 0;
    delete this->display_elemets;
    this->display_elemets = nullptr;
  }
} js_array_t;

typedef struct Oddball: inspect_t {
  std::string value = "";
  ~Oddball() {
    this->value = "";
  }
} oddball_t;

typedef struct JsFunction: inspect_t {
  std::string func_name = "";
  std::string func_source = "";
  std::string debug_line = "";
  std::string context_address = "";
  inspect_t* context = nullptr;
  ~JsFunction() {
    this->func_name = "";
    this->func_source = "";
    this->debug_line = "";
    this->context_address = "";
    delete this->context;
    this->context = nullptr;
  }
} js_function_t;

typedef struct Context: inspect_t {
  std::string previous_address = "";
  std::string closure_address = "";
  inspect_t* closure = nullptr;
  properties_t* scope_object = nullptr;
  ~Context() {
    this->previous_address = "";
    this->closure_address = "";
    delete this->closure;
    this->closure = nullptr;
    delete this->scope_object;
    this->scope_object = nullptr;
  }
} context_t;

typedef struct JsRegexp: inspect_t {
  std::string source = "";
  elements_t* elements = nullptr;
  properties_t* properties = nullptr;
  ~JsRegexp() {
    this->source = "";
    delete this->elements;
    this->elements = nullptr;
    delete this->properties;
    this->properties = nullptr;
  }
} js_regexp_t;

typedef struct FirstNonString: inspect_t {
  int total_length = 0;
  std::string display_value = "";
  int current = 0;
  bool end = false;
  ~FirstNonString() {
    this->total_length = 0;
    this->display_value = "";
    this->current = 0;
    this->end = false;
  }
} first_non_string_t;

typedef struct JsArrayBuffer: inspect_t {
  // if true, show "[neutered]"
  bool neutered = false;
  int byte_length = 0;
  std::string backing_store_address = "";
  int display_length = 0;
  std::string* elements = nullptr;
  ~JsArrayBuffer() {
    this->neutered = false;
    this->byte_length = 0;
    this->backing_store_address = "";
    this->display_length = 0;
    delete[] elements;
    this->elements = nullptr;
  }
} js_array_buffer_t;

typedef struct JsArrayBufferView: inspect_t {
  // if true, show "[neutered]"
  bool neutered = false;
  int byte_length = 0;
  int byte_offset = 0;
  std::string backing_store_address = "";
  int display_length = 0;
  std::string* elements = nullptr;
  ~JsArrayBufferView() {
    this->neutered = false;
    this->byte_length = 0;
    this->byte_offset = 0;
    this->backing_store_address = "";
    this->display_length = 0;
    delete[] this->elements;
    this->elements = nullptr;
  }
} js_array_buffer_view_t;

typedef struct JsDate: inspect_t {
  std::string value = "";
  ~JsDate() {
    this->value = "";
  }
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
