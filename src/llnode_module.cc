// Javascript module API for llnode/lldb
#include <node.h>
#include "src/llnode_api.h"

namespace llnode {

using v8::FunctionCallbackInfo;
using v8::Exception;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Number;

void LoadDump(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() < 1) {
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }

  String::Utf8Value filename(args[0]);
  initSBTarget(*filename);
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, "OK"));
}

void GetThreadCount(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  args.GetReturnValue().Set(Number::New(isolate, getSBThreadCount()));
}

void GetFrameCount(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    args.GetReturnValue().Set(Number::New(isolate, getSBFrameCount(args[0]->NumberValue())));
}

void GetFrame(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  char buffer[4096];

  getSBFrame(args[0]->NumberValue(), args[1]->NumberValue(), 4096, buffer);
  args.GetReturnValue().Set(String::NewFromUtf8(isolate, buffer));
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "loadDump", LoadDump);
  NODE_SET_METHOD(exports, "getThreadCount", GetThreadCount);
  NODE_SET_METHOD(exports, "getFrameCount", GetFrameCount);
  NODE_SET_METHOD(exports, "getFrame", GetFrame);
}

NODE_MODULE(llnode_module, init)

}  // namespace llnode

