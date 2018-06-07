#include "src/llnode-module.h"

namespace llnode {
using ::v8::Object;
using ::v8::Value;
using ::v8::String;
using ::v8::Local;
using ::v8::Function;
using ::v8::FunctionTemplate;

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

void LLNode::LoadCore(const Nan::FunctionCallbackInfo<Value>& info) {
  LLNode* llnode = ObjectWrap::Unwrap<LLNode>(info.Holder());
  int err = llnode->api->LoadCore();
  printf("load core result: %d\n", err);
}
}
