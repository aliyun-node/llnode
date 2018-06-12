#include "src/llnode-module.h"

namespace llnode {
using ::v8::Object;
using ::v8::Value;
using ::v8::String;
using ::v8::Local;
using ::v8::Function;
using ::v8::Number;
using ::v8::Array;
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

Local<Array> LLNode::GetThreadInfoById(size_t thread_index, size_t curt, size_t limt) {
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
    if(ft->type == 1) {
      native_frame_t* nft = ft->native_frame;
      frame->Set(Nan::New<String>("symbol").ToLocalChecked(), Nan::New<String>(nft->symbol).ToLocalChecked());
      frame->Set(Nan::New<String>("function").ToLocalChecked(), Nan::New<String>(nft->function).ToLocalChecked());
      frame->Set(Nan::New<String>("module").ToLocalChecked(), Nan::New<String>(nft->module_file).ToLocalChecked());
      frame->Set(Nan::New<String>("compile_unit").ToLocalChecked(), Nan::New<String>(nft->compile_unit_file).ToLocalChecked());
    }
    if(ft->type == 2) {
      js_frame_t* jft = ft->js_frame;
      frame->Set(Nan::New<String>("symbol").ToLocalChecked(), Nan::New<String>(jft->symbol).ToLocalChecked());
      if(jft->type == 1) {
        std::string invalid_js_frame = jft->invalid_js_frame;
        frame->Set(Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(invalid_js_frame).ToLocalChecked());
      }
      if(jft->type == 2) {
        frame->Set(Nan::New<String>("function").ToLocalChecked(), Nan::New<String>(jft->valid_js_frame->function).ToLocalChecked());
        frame->Set(Nan::New<String>("context").ToLocalChecked(), Nan::New<String>(jft->valid_js_frame->context).ToLocalChecked());
        frame->Set(Nan::New<String>("arguments").ToLocalChecked(), Nan::New<String>(jft->valid_js_frame->arguments).ToLocalChecked());
        frame->Set(Nan::New<String>("line").ToLocalChecked(), Nan::New<String>(jft->valid_js_frame->line).ToLocalChecked());
        frame->Set(Nan::New<String>("func_addr").ToLocalChecked(), Nan::New<String>(jft->valid_js_frame->address).ToLocalChecked());
      }
    }
    frame_list->Set(frame_index - current, frame);
  }
  return frame_list;
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
  uint32_t current = 0;
  if(info[0]->IsNumber())
    current = Nan::To<uint32_t>(info[0]).FromJust();
  uint32_t limit = 0;
  if(info[1]->IsNumber())
    limit = Nan::To<uint32_t>(info[1]).FromJust();
  else
    limit = type_count;
  if(current >= type_count)
    current = type_count;
  uint32_t end = current + limit;
  if(end >= type_count)
    end = type_count;
  Local<Array> object_list = Nan::New<Array>(end - current);
  for(uint32_t i = current; i < end; ++i) {
    Local<Object> type = Nan::New<Object>();
    type->Set(Nan::New<String>("index").ToLocalChecked(), Nan::New<Number>(i));
    type->Set(Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(llnode->api->GetTypeName(i)).ToLocalChecked());
    type->Set(Nan::New<String>("count").ToLocalChecked(), Nan::New<Number>(llnode->api->GetTypeInstanceCount(i)));
    type->Set(Nan::New<String>("size").ToLocalChecked(), Nan::New<Number>(llnode->api->GetTypeTotalSize(i)));
    object_list->Set(i - current, type);
  }
  info.GetReturnValue().Set(object_list);
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
  Local<Array> result = Nan::New<Array>(end - current);
  for(uint32_t i = current; i < end; ++i) {
    Local<Object> ins = Nan::New<Object>();
    std::string addr_str = *instances[i];
    ins->Set(Nan::New<String>("address").ToLocalChecked(), Nan::New<String>(addr_str).ToLocalChecked());
    uint64_t addr = std::strtoull(addr_str.c_str(), nullptr, 16);
    std::string desc = llnode->api->GetObject(addr, false);
    ins->Set(Nan::New<String>("desc").ToLocalChecked(), Nan::New<String>(desc).ToLocalChecked());
    result->Set(i - current, ins);
  }
  delete pagination;
  info.GetReturnValue().Set(result);
}
}
