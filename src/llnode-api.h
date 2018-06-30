#ifndef SRC_LLNODE_API_H
#define SRC_LLNODE_API_H

#include <vector>
#include <unordered_map>
#include <cinttypes>

#include "src/llnode-common.h"

namespace lldb {
class SBDebugger;
class SBTarget;
class SBProcess;
}

namespace llnode {
class LLNode;
class LLScan;
class TypeRecord;

namespace v8 {
class LLV8;
}

typedef std::unordered_map<long long, frame_t*> FrameMap;
typedef std::unordered_map<std::string, inspect_t*> InspectMap;
typedef std::unordered_map<size_t, std::string**> InstancesMap;

class LLNodeApi {
public:
  LLNodeApi(LLNode* llnode);
  ~LLNodeApi();
  int LoadCore();
  uint32_t GetProcessID();
  uint32_t GetThreadCount();
  std::string GetProcessState();
  std::string GetExecutableName();
  std::string GetProcessInfo();
  std::string GetThreadStopReason(size_t thread_index);
  uint64_t GetThreadID(size_t thread_index);
  std::string GetThreadName(size_t thread_index);
  std::string GetThreadStartAddress(size_t thread_index);
  uint32_t GetFrameCountByThreadId(size_t thread_index);
  frame_t* GetFrameInfo(size_t thread_index, size_t frame_index);
  bool ScanHeap();
  void CacheAndSortHeap();
  uint32_t GetHeapTypeCount();
  std::string GetTypeName(size_t type_index);
  uint32_t GetTypeInstanceCount(size_t type_index);
  uint32_t GetTypeTotalSize(size_t type_index);
  std::string** GetTypeInstances(size_t type_index);
  std::string GetObject(uint64_t address, bool detailed);
  inspect_t* Inspect(uint64_t address, bool detailed, unsigned int current = 0, unsigned int limit = 0);

private:
  LLNode* llnode;
  bool debugger_initialized = false;
  bool core_loaded = false;
  std::unique_ptr<lldb::SBDebugger> debugger;
  std::unique_ptr<lldb::SBTarget> target;
  std::unique_ptr<lldb::SBProcess> process;
  std::unique_ptr<v8::LLV8> llv8;
  std::unique_ptr<LLScan> llscan;
  std::vector<TypeRecord*> object_types;
  FrameMap frame_map;
  InspectMap inspect_map;
  InstancesMap instances_map;
};
}

#endif
