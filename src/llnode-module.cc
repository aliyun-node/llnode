#include "src/llnode-module.h"

namespace llnode {
using ::v8::Object;
using ::v8::Value;
using ::v8::String;
using ::v8::Local;
using ::v8::Function;
using ::v8::Number;
using ::v8::Array;
using ::v8::Boolean;
using ::v8::FunctionTemplate;

template <typename T>
pagination_t<T>* GetPagination(Local<Value> in_curt, Local<Value> in_limt, T length) {
  pagination_t<T>* pagination = new pagination_t<T>;
  T current = 0;
  if(in_curt->IsNumber())
    current = static_cast<T>(in_curt->ToInteger()->Value());
  T limit = 0;
  if(in_limt->IsNumber())
    limit = static_cast<T>(in_limt->ToInteger()->Value());
  else
    limit = length;
  if(current >= length)
    current = length;
  T end = current + limit;
  if(end >= length)
    end = length;
  pagination->current = current;
  pagination->end = end;
  return pagination;
}

Nan::Persistent<Function> LLNode::constructor;

LLNode::LLNode(char* core_path, char* executable_path)
  : api(new LLNodeApi(this)) {
  core = new core_wrap_t;
  int core_path_length = strlen(core_path) + 1;
  core->core = new char[core_path_length];
  strncpy(core->core, core_path, core_path_length);
  int executable_path_length = strlen(executable_path) + 1;
  core->executable = new char[executable_path_length];
  strncpy(core->executable, executable_path, executable_path_length);
}
LLNode::~LLNode() {}

void LLNode::Init(Local<Object> exports) {
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New<String>("LLNode").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // set prototype
  Nan::SetPrototypeMethod(tpl, "loadCore", LoadCore);
  Nan::SetPrototypeMethod(tpl, "getProcessInfo", GetProcessInfo);
  Nan::SetPrototypeMethod(tpl, "getThreadByIds", GetThreadByIds);
  Nan::SetPrototypeMethod(tpl, "getJsObjects", GetJsObjects);
  Nan::SetPrototypeMethod(tpl, "getJsInstances", GetJsInstances);
  Nan::SetPrototypeMethod(tpl, "inspectJsObjectAtAddress", InspectJsObjectAtAddress);
  // return js class
  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("LLNode").ToLocalChecked(), tpl->GetFunction());
}

void LLNode::New(const Nan::FunctionCallbackInfo<Value>& info) {
  if(!info[0]->IsString() || !info[1]->IsString()) {
    Nan::ThrowTypeError(Nan::New<String>("core path and executable path must be string!").ToLocalChecked());
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }
  // new instance
  if (info.IsConstructCall()) {
    Nan::Utf8String core_path(info[0]->ToString());
    Nan::Utf8String executable_path(info[1]->ToString());
    LLNode* llnode = new LLNode(*core_path, *executable_path);
    llnode->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    const int argc = 2;
    Local<Value> argv[argc] = { info[0], info[1] };
    Local<Function> cons = Nan::New<Function>(constructor);
    info.GetReturnValue().Set((Nan::NewInstance(cons, argc, argv)).ToLocalChecked());
  }
}

core_wrap_t* LLNode::GetCore() {
  return core;
}

bool LLNode::ScanHeap() {
  if (!heap_initialized) {
    // TODO: scan heap needed working in child thread if we can
    if(!api->ScanHeap()) {
      return false;
    }
    api->CacheAndSortHeap();
    heap_initialized = true;
  }
  return true;
}

Local<Object> LLNode::GetThreadInfoById(size_t thread_index, size_t curt, size_t limt) {
  Local<Object> result = Nan::New<Object>();
  uint32_t frames = api->GetFrameCountByThreadId(thread_index);
  // pagination
  size_t current = 0;
  if(curt > 0)
    current = curt;
  size_t limit = 0;
  if(limt > 0)
    limit = limt;
  else
    limit = frames;
  if(current >= frames)
    current = frames;
  size_t end = current + limit;
  if(end >= frames)
    end = frames;
  // set thread frame
  Local<Array> frame_list = Nan::New<Array>(end - current);
  for(size_t frame_index = current; frame_index < end; ++frame_index) {
    Local<Object> frame = Nan::New<Object>();
    frame_t* ft = api->GetFrameInfo(thread_index, frame_index);
    frame->Set(Nan::New<String>("type").ToLocalChecked(),
               Nan::New<Number>(ft->type));
    frame->Set(Nan::New<String>("name").ToLocalChecked(),
               Nan::New<String>(ft->name).ToLocalChecked());
    frame->Set(Nan::New<String>("function").ToLocalChecked(),
               Nan::New<String>(ft->function).ToLocalChecked());
    if(ft->type == FrameType::kNativeFrame) {
      native_frame_t* nft = static_cast<native_frame_t*>(ft);
      frame->Set(Nan::New<String>("module").ToLocalChecked(),
                 Nan::New<String>(nft->module_file).ToLocalChecked());
      frame->Set(Nan::New<String>("compile_unit").ToLocalChecked(),
                 Nan::New<String>(nft->compile_unit_file).ToLocalChecked());
    }
    if(ft->type == FrameType::kJsFrame) {
      js_frame_t* jft = static_cast<js_frame_t*>(ft);
      if(jft->args != nullptr) {
        if(jft->args->context != nullptr)
          frame->Set(Nan::New<String>("context").ToLocalChecked(), InspectJsObject(jft->args->context));
        if(jft->args->args_list != nullptr) {
          Local<Array> args_list = Nan::New<Array>(jft->args->length);
          for(int i = 0; i < jft->args->length; ++i) {
            if(jft->args->args_list[i] != nullptr)
              args_list->Set(i, InspectJsObject(jft->args->args_list[i]));
            else
              args_list->Set(i, Nan::Null());
          }
          frame->Set(Nan::New<String>("arguments").ToLocalChecked(), args_list);
        }
      }
      if(jft->debug != nullptr)
        frame->Set(
          Nan::New<String>("line").ToLocalChecked(),
          Nan::New<String>(jft->debug->line).ToLocalChecked());
      frame->Set(
        Nan::New<String>("func_addr").ToLocalChecked(),
        Nan::New<String>(jft->address).ToLocalChecked());
    }
    frame_list->Set(frame_index - current, frame);
  }
  if(end >= frames)
    result->Set(Nan::New<String>("frame_end").ToLocalChecked(), Nan::New<Boolean>(true));
  else {
    result->Set(Nan::New<String>("frame_end").ToLocalChecked(), Nan::New<Boolean>(false));
    result->Set(Nan::New<String>("frame_left").ToLocalChecked(), Nan::New<Number>(frames - end));
  }
  result->Set(Nan::New<String>("frame_list").ToLocalChecked(), frame_list);
  return result;
}

Local<Object> LLNode::InspectJsObject(inspect_t* inspect) {
  InspectType type = inspect->type;
  std::string name = inspect->name;
  std::string address = inspect->address;
  std::string map_address = inspect->map_address;
  Local<Object> result = Nan::New<Object>();
  result->Set(Nan::New<String>("type").ToLocalChecked(), Nan::New<Number>(type));
  result->Set(Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(name).ToLocalChecked());
  result->Set(Nan::New<String>("address").ToLocalChecked(), Nan::New<String>(address).ToLocalChecked());
  result->Set(Nan::New<String>("map_address").ToLocalChecked(), Nan::New<String>(map_address).ToLocalChecked());
  switch(type) {
  case InspectType::kGlobalObject:
  case InspectType::kGlobalProxy:
  case InspectType::kCode:
  case InspectType::kUnknown:
    break;
  case InspectType::kSmi: {
    smi_t* smi = static_cast<smi_t*>(inspect);
    result->Set(Nan::New<String>("value").ToLocalChecked(),
                Nan::New<String>(smi->value).ToLocalChecked());
    break;
  }
  case InspectType::kMap: {
    map_t* map = static_cast<map_t*>(inspect);
    result->Set(Nan::New<String>("constructor").ToLocalChecked(),
                Nan::New<String>(map->in_object_properties_or_constructor).ToLocalChecked());
    result->Set(Nan::New<String>("constructor_index").ToLocalChecked(),
                Nan::New<Number>(map->in_object_properties_or_constructor_index));
    result->Set(Nan::New<String>("size").ToLocalChecked(),
                Nan::New<Number>(map->instance_size));
    result->Set(Nan::New<String>("descriptors_address").ToLocalChecked(),
                Nan::New<String>(map->descriptors_address).ToLocalChecked());
    result->Set(Nan::New<String>("descriptors_length").ToLocalChecked(),
                Nan::New<Number>(map->own_descriptors));
    result->Set(Nan::New<String>("descriptors").ToLocalChecked(),
                InspectJsObject(map->descriptors_array));
    break;
  }
  case InspectType::kFixedArray: {
    fixed_array_t* fixed_array = static_cast<fixed_array_t*>(inspect);
    Local<Array> array = Nan::New<Array>(fixed_array->length);
    for(int i = 0; i < fixed_array->length; ++i) {
      array->Set(i, InspectJsObject(*(fixed_array->content + i)));
    }
    result->Set(Nan::New<String>("length").ToLocalChecked(),
                Nan::New<Number>(fixed_array->length));
    result->Set(Nan::New<String>("array").ToLocalChecked(), array);
    break;
  }
  case InspectType::kJsObject: {
    js_object_t* js_object = static_cast<js_object_t*>(inspect);
    result->Set(Nan::New<String>("constructor").ToLocalChecked(),
                Nan::New<String>(js_object->constructor).ToLocalChecked());
    if(js_object->elements != nullptr) {
      Local<Array> elements = Nan::New<Array>(js_object->elements->length);
      for(int i = 0; i < js_object->elements->length; ++i) {
        elements->Set(i, InspectJsObject(*(js_object->elements->elements + i)));
      }
      result->Set(Nan::New<String>("elements").ToLocalChecked(), elements);
    }
    if(js_object->properties != nullptr) {
      Local<Object> properties = Nan::New<Object>();
      properties_t* props = js_object->properties;
      for(int i = 0; i < props->length; ++i) {
        property_t* prop = *(props->properties + i);
        // ignore hole
        if(prop == nullptr) continue;
        if(prop->value == nullptr)
          properties->Set(Nan::New<String>(prop->key).ToLocalChecked(),
                          Nan::New<String>(prop->value_str).ToLocalChecked());
        else
          properties->Set(Nan::New<String>(prop->key).ToLocalChecked(),
                          InspectJsObject(prop->value));
      }
      result->Set(Nan::New<String>("properties").ToLocalChecked(), properties);
    }
    if(js_object->fields != nullptr) {
      internal_fileds_t* ifield = js_object->fields;
      Local<Array> fields = Nan::New<Array>(ifield->length);
      for(int i = 0; i < ifield->length; ++i) {
        fields->Set(i, Nan::New<String>((*(ifield->internal_fileds + i))->address).ToLocalChecked());
      }
      result->Set(Nan::New<String>("internal_fields").ToLocalChecked(), fields);
    }
    break;
  }
  case InspectType::kHeapNumber: {
    heap_number_t* heap_number = static_cast<heap_number_t*>(inspect);
    result->Set(Nan::New<String>("value").ToLocalChecked(),
                Nan::New<String>(heap_number->value).ToLocalChecked());
    break;
  }
  case InspectType::kJsArray: {
    js_array_t* js_array = static_cast<js_array_t*>(inspect);
    result->Set(Nan::New<String>("total_length").ToLocalChecked(),
                Nan::New<Number>(js_array->total_length));
    Local<Array> array = Nan::New<Array>(js_array->display_elemets->length);
    for(int i = 0; i < js_array->display_elemets->length; ++i) {
      array->Set(i, InspectJsObject(*(js_array->display_elemets->elements + i)));
    }
    result->Set(Nan::New<String>("display_array").ToLocalChecked(), array);
    break;
  }
  case InspectType::kOddball: {
    oddball_t* oddball = static_cast<oddball_t*>(inspect);
    result->Set(Nan::New<String>("value").ToLocalChecked(),
                Nan::New<String>(oddball->value).ToLocalChecked());
    break;
  }
  case InspectType::kJsFunction: {
    js_function_t* js_function = static_cast<js_function_t*>(inspect);
    result->Set(Nan::New<String>("func_name").ToLocalChecked(),
                Nan::New<String>(js_function->func_name).ToLocalChecked());
    result->Set(Nan::New<String>("func_source").ToLocalChecked(),
                Nan::New<String>(js_function->func_source).ToLocalChecked());
    result->Set(Nan::New<String>("debug_line").ToLocalChecked(),
                Nan::New<String>(js_function->debug_line).ToLocalChecked());
    result->Set(Nan::New<String>("context_address").ToLocalChecked(),
                Nan::New<String>(js_function->context_address).ToLocalChecked());
    if(js_function->context != nullptr)
      result->Set(Nan::New<String>("context").ToLocalChecked(),
                  InspectJsObject(js_function->context));
    break;
  }
  case InspectType::kJsRegExp: {
    js_regexp_t* js_regexp = static_cast<js_regexp_t*>(inspect);
    result->Set(Nan::New<String>("regexp").ToLocalChecked(),
                Nan::New<String>(js_regexp->source).ToLocalChecked());
    if(js_regexp->elements != nullptr) {
      Local<Array> elements = Nan::New<Array>(js_regexp->elements->length);
      for(int i = 0; i < js_regexp->elements->length; ++i) {
        elements->Set(i, InspectJsObject(*(js_regexp->elements->elements + i)));
      }
      result->Set(Nan::New<String>("elements").ToLocalChecked(), elements);
    }
    if(js_regexp->properties != nullptr) {
      Local<Object> properties = Nan::New<Object>();
      properties_t* props = js_regexp->properties;
      for(int i = 0; i < props->length; ++i) {
        property_t* prop = *(props->properties + i);
        // ignore hole
        if(prop == nullptr) continue;
        if(prop->value == nullptr)
          properties->Set(Nan::New<String>(prop->key).ToLocalChecked(),
                          Nan::New<String>(prop->value_str).ToLocalChecked());
        else
          properties->Set(Nan::New<String>(prop->key).ToLocalChecked(),
                          InspectJsObject(prop->value));
      }
      result->Set(Nan::New<String>("properties").ToLocalChecked(), properties);
    }
    break;
  }
  case InspectType::kFirstNonstring: {
    first_non_string_t* non_string = static_cast<first_non_string_t*>(inspect);
    result->Set(Nan::New<String>("total_length").ToLocalChecked(),
                Nan::New<Number>(non_string->total_length));
    result->Set(Nan::New<String>("display").ToLocalChecked(),
                Nan::New<String>(non_string->display_value).ToLocalChecked());
    break;
  }
  case InspectType::kJsArrayBuffer: {
    js_array_buffer_t* array_buffer = static_cast<js_array_buffer_t*>(inspect);
    result->Set(Nan::New<String>("neutered").ToLocalChecked(),
                Nan::New<Boolean>(array_buffer->neutered));
    if(!array_buffer->neutered) {
      result->Set(Nan::New<String>("byte_length").ToLocalChecked(),
                  Nan::New<Number>(array_buffer->byte_length));
      result->Set(Nan::New<String>("backing_store_address").ToLocalChecked(),
                  Nan::New<String>(array_buffer->backing_store_address).ToLocalChecked());
      if(array_buffer->elements != nullptr) {
        Local<Array> elements = Nan::New<Array>(array_buffer->display_length);
        for(int i = 0; i < array_buffer->display_length; ++i) {
          elements->Set(i, Nan::New<String>(*(array_buffer->elements + i)).ToLocalChecked());
        }
        result->Set(Nan::New<String>("display_array").ToLocalChecked(), elements);
      }
    }
    break;
  }
  case InspectType::kJsArrayBufferView: {
    js_array_buffer_view_t* array_buffer_view = static_cast<js_array_buffer_view_t*>(inspect);
    result->Set(Nan::New<String>("neutered").ToLocalChecked(),
                Nan::New<Boolean>(array_buffer_view->neutered));
    if(!array_buffer_view->neutered) {
      result->Set(Nan::New<String>("byte_length").ToLocalChecked(),
                  Nan::New<Number>(array_buffer_view->byte_length));
      result->Set(Nan::New<String>("byte_offset").ToLocalChecked(),
                  Nan::New<Number>(array_buffer_view->byte_offset));
      result->Set(Nan::New<String>("backing_store_address").ToLocalChecked(),
                  Nan::New<String>(array_buffer_view->backing_store_address).ToLocalChecked());
      if(array_buffer_view->elements != nullptr) {
        Local<Array> elements = Nan::New<Array>(array_buffer_view->display_length);
        for(int i = 0; i < array_buffer_view->display_length; ++i) {
          elements->Set(i, Nan::New<String>(*(array_buffer_view->elements + i)).ToLocalChecked());
        }
        result->Set(Nan::New<String>("display_array").ToLocalChecked(), elements);
      }
    }
    break;
  }
  case InspectType::kJsDate: {
    js_date_t* js_date = static_cast<js_date_t*>(inspect);
    result->Set(Nan::New<String>("date").ToLocalChecked(),
                Nan::New<String>(js_date->value).ToLocalChecked());
    break;
  }
  default:
    break;
  }

  return result;
}

void LLNode::LoadCore(const Nan::FunctionCallbackInfo<Value>& info) {
  LLNode* llnode = ObjectWrap::Unwrap<LLNode>(info.Holder());
  int err = llnode->api->LoadCore();
  if(err == 1) {
    std::string executable = llnode->core->executable;
    std::string executable_invaild = "executable [" + executable + "] is not valid!";
    Nan::ThrowError(Nan::New<String>(executable_invaild).ToLocalChecked());
    return;
  }
  if(err == 2) {
    std::string core = llnode->core->core;
    std::string core_invaild = "coredump file [" + core + "] is not valid!";
    Nan::ThrowError(Nan::New<String>(core_invaild).ToLocalChecked());
    return;
  }
  info.GetReturnValue().Set(Nan::New<Number>(err));
}

void LLNode::GetProcessInfo(const Nan::FunctionCallbackInfo<Value>& info) {
  LLNode* llnode = ObjectWrap::Unwrap<LLNode>(info.Holder());
  Local<Object> result = Nan::New<Object>();
  uint32_t pid = llnode->api->GetProcessID();
  result->Set(Nan::New<String>("pid").ToLocalChecked(), Nan::New<Number>(pid));
  uint32_t thread_count = llnode->api->GetThreadCount();
  result->Set(Nan::New<String>("thread_count").ToLocalChecked(), Nan::New<Number>(thread_count));
  std::string state = llnode->api->GetProcessState();
  result->Set(Nan::New<String>("state").ToLocalChecked(), Nan::New<String>(state).ToLocalChecked());
  std::string executable = llnode->core->executable;
  result->Set(Nan::New<String>("executable").ToLocalChecked(), Nan::New<String>(executable).ToLocalChecked());
  info.GetReturnValue().Set(result);
}

void LLNode::GetThreadByIds(const Nan::FunctionCallbackInfo<Value>& info) {
  if(!info[0]->IsArray() && !info[0]->IsNumber()) {
    Nan::ThrowTypeError(Nan::New<String>("thread index(list) must be array or number!").ToLocalChecked());
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }
  LLNode* llnode = ObjectWrap::Unwrap<LLNode>(info.Holder());
  size_t current = 0;
  if(info[1]->IsNumber())
    current = static_cast<size_t>(info[1]->ToInteger()->Value());
  size_t limit = 0;
  if(info[2]->IsNumber())
    limit = static_cast<size_t>(info[2]->ToInteger()->Value());
  if(info[0]->IsArray()) {
    Local<Object> list = info[0]->ToObject();
    int length = list->Get(Nan::New<String>("length").ToLocalChecked())->ToInteger()->Value();
    Local<Array> result = Nan::New<Array>(length);
    for(int i = 0; i < length; ++i) {
      size_t thread_index = static_cast<size_t>(list->Get(Nan::New<Number>(i))->ToInteger()->Value());
      result->Set(i, llnode->GetThreadInfoById(thread_index, current, limit));
    }
    info.GetReturnValue().Set(result);
  } else if(info[0]->IsNumber()) {
    Local<Array> result = Nan::New<Array>(1);
    result->Set(0, llnode->GetThreadInfoById(static_cast<size_t>(info[0]->ToInteger()->Value()), current, limit));
    info.GetReturnValue().Set(result);
  }
}

void LLNode::GetJsObjects(const Nan::FunctionCallbackInfo<Value>& info) {
  LLNode* llnode = ObjectWrap::Unwrap<LLNode>(info.Holder());
  if(!llnode->ScanHeap()) {
    Nan::ThrowTypeError(Nan::New<String>("scan heap error!").ToLocalChecked());
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }
  uint32_t type_count = llnode->api->GetHeapTypeCount();
  pagination_t<uint32_t>* pagination = GetPagination<uint32_t>(info[0], info[1], type_count);
  uint32_t current = pagination->current;
  uint32_t end = pagination->end;
  Local<Object> result = Nan::New<Object>();
  Local<Array> object_list = Nan::New<Array>(end - current);
  for(uint32_t i = current; i < end; ++i) {
    Local<Object> type = Nan::New<Object>();
    type->Set(Nan::New<String>("index").ToLocalChecked(), Nan::New<Number>(i));
    type->Set(Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(llnode->api->GetTypeName(i)).ToLocalChecked());
    type->Set(Nan::New<String>("count").ToLocalChecked(), Nan::New<Number>(llnode->api->GetTypeInstanceCount(i)));
    type->Set(Nan::New<String>("size").ToLocalChecked(), Nan::New<Number>(llnode->api->GetTypeTotalSize(i)));
    object_list->Set(i - current, type);
  }
  delete pagination;
  if(end >= type_count)
    result->Set(Nan::New<String>("object_end").ToLocalChecked(), Nan::New<Boolean>(true));
  else {
    result->Set(Nan::New<String>("object_end").ToLocalChecked(), Nan::New<Boolean>(false));
    result->Set(Nan::New<String>("object_left").ToLocalChecked(), Nan::New<Number>(type_count - end));
  }
  result->Set(Nan::New<String>("object_list").ToLocalChecked(), object_list);
  info.GetReturnValue().Set(result);
}
void LLNode::GetJsInstances(const Nan::FunctionCallbackInfo<Value>& info) {
  if(!info[0]->IsNumber()) {
    Nan::ThrowTypeError(Nan::New<String>("instance index must be number!").ToLocalChecked());
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }
  LLNode* llnode = ObjectWrap::Unwrap<LLNode>(info.Holder());
  if(!llnode->ScanHeap()) {
    Nan::ThrowTypeError(Nan::New<String>("scan heap error!").ToLocalChecked());
    info.GetReturnValue().Set(Nan::Undefined());
    return;
  }
  size_t instance_index = static_cast<size_t>(info[0]->ToInteger()->Value());
  uint32_t instance_count = llnode->api->GetTypeInstanceCount(instance_index);
  pagination_t<uint32_t>* pagination = GetPagination<uint32_t>(info[1], info[2], instance_count);
  uint32_t current = pagination->current;
  uint32_t end = pagination->end;
  std::string** instances = llnode->api->GetTypeInstances(instance_index);
  Local<Object> result = Nan::New<Object>();
  Local<Array> instance_list = Nan::New<Array>(end - current);
  for(uint32_t i = current; i < end; ++i) {
    Local<Object> ins = Nan::New<Object>();
    std::string addr_str = *instances[i];
    ins->Set(Nan::New<String>("address").ToLocalChecked(), Nan::New<String>(addr_str).ToLocalChecked());
    uint64_t addr = std::strtoull(addr_str.c_str(), nullptr, 16);
    std::string desc = llnode->api->GetObject(addr, false);
    ins->Set(Nan::New<String>("desc").ToLocalChecked(), Nan::New<String>(desc).ToLocalChecked());
    instance_list->Set(i - current, ins);
  }
  delete pagination;
  if(end >= instance_count)
    result->Set(Nan::New<String>("instance_end").ToLocalChecked(), Nan::New<Boolean>(true));
  else {
    result->Set(Nan::New<String>("instance_end").ToLocalChecked(), Nan::New<Boolean>(false));
    result->Set(Nan::New<String>("instance_left").ToLocalChecked(), Nan::New<Number>(instance_count - end));
  }
  result->Set(Nan::New<String>("instance_list").ToLocalChecked(), instance_list);
  info.GetReturnValue().Set(result);
}

void LLNode::InspectJsObjectAtAddress(const Nan::FunctionCallbackInfo<Value>& info) {
  Nan::Utf8String address_str(info[0]);
  if ((*address_str)[0] != '0' || (*address_str)[1] != 'x' ||
      address_str.length() > 18) {
    Nan::ThrowTypeError("Invalid address");
    return;
  }
  LLNode* llnode = ObjectWrap::Unwrap<LLNode>(info.Holder());
  uint64_t addr = std::strtoull(*address_str, nullptr, 16);
  Local<Object> result = llnode->InspectJsObject(llnode->api->Inspect(addr, true));
  info.GetReturnValue().Set(result);
}
}
