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
using ::v8::Array;

typedef struct {
  char* core;
  char* executable;
} core_wrap_t;

template <typename T>
struct pagination_t {
  T current;
  T end;
};
template <typename T>
pagination_t<T>* GetPagination(Local<Value> in_curt, Local<Value> in_limt, T length);

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
  static void GetProcessInfo(const Nan::FunctionCallbackInfo<Value>& info);
  static void GetThreadByIds(const Nan::FunctionCallbackInfo<Value>& info);
  static void GetJsObjects(const Nan::FunctionCallbackInfo<Value>& info);
  static void GetJsInstances(const Nan::FunctionCallbackInfo<Value>& info);
  static void InspectJsObjectAtAddress(const Nan::FunctionCallbackInfo<Value>& info);
  Local<Array> GetThreadInfoById(size_t thread_index, size_t curt, size_t limt);
  Local<Object> InspectJsObject(inspect_t* inspect);
  bool ScanHeap();

  // core & executable
  LLNodeApi* api;
  core_wrap_t* core;
  // lazy heap scanning
  bool heap_initialized = false;
};
}

#endif
