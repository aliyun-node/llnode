#ifndef SRC_LLNODE_API_H
#define SRC_LLNODE_API_H

#include <vector>

namespace lldb {
class SBDebugger;
class SBTarget;
class SBProcess;
}

namespace llnode {
class LLNode;
class LLScan;

namespace v8 {
class LLV8;
}

class LLNodeApi {
public:
  LLNodeApi(LLNode* llnode);
  ~LLNodeApi();
  int LoadCore();

private:
  LLNode* llnode;
  bool debugger_initialized = false;
  bool core_loaded = false;
  std::unique_ptr<lldb::SBDebugger> debugger;
  std::unique_ptr<lldb::SBTarget> target;
  std::unique_ptr<lldb::SBProcess> process;
  std::unique_ptr<v8::LLV8> llv8;
  std::unique_ptr<LLScan> llscan;
};

}

#endif
