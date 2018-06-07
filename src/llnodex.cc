#include <nan.h>
#include "llnode-module.h"

namespace llnode {
void InitAll(::v8::Local<::v8::Object> exports) {
  LLNode::Init(exports);
}

NODE_MODULE(llnodex, InitAll)
}
