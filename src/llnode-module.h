#ifndef SRC_LLNODE_MODULE_H
#define SRC_LLNODE_MODULE_H

#include <nan.h>
#include <memory>
#include "src/llnode-api.h"

namespace llnode {
using ::v8::Local;
using ::v8::Object;
using ::v8::Value;
using ::v8::Function;


typedef struct {
  char* core;
  char* executable;
} core_wrap_t;

class LLNode : public Nan::ObjectWrap {
public:
  static void Init(Local<Object> exports);
  static void NewInstance(const Nan::FunctionCallbackInfo<Value>& info);
  core_wrap_t* GetCore();

private:
  explicit LLNode(char* core_path, char* executable_path);
  ~LLNode();
  static Nan::Persistent<Function> constructor;
  static void New(const Nan::FunctionCallbackInfo<Value>& info);
  static void LoadCore(const Nan::FunctionCallbackInfo<Value>& info);
  // core & executable
  LLNodeApi* api;
  core_wrap_t* core;
};
}

#endif
