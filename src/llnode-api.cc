#include <lldb/API/SBCommandReturnObject.h>
#include <lldb/API/SBCompileUnit.h>
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBFileSpec.h>
#include <lldb/API/SBFrame.h>
#include <lldb/API/SBModule.h>
#include <lldb/API/SBProcess.h>
#include <lldb/API/SBSymbol.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBThread.h>
#include <lldb/lldb-enumerations.h>

#include "src/error.h"
#include "src/llnode-api.h"
#include "src/llnode-module.h"
#include "src/llscan.h"
#include "src/llv8.h"

namespace llnode {
using lldb::SBCommandReturnObject;
using lldb::SBDebugger;
using lldb::SBFrame;
using lldb::SBLineEntry;
using lldb::SBProcess;
using lldb::SBStream;
using lldb::SBSymbol;
using lldb::SBTarget;
using lldb::SBThread;
using lldb::StateType;
using std::set;
using std::string;
using v8::LLV8;

LLNodeApi::LLNodeApi(LLNode* llnode)
    : llnode(llnode),
      debugger(new SBDebugger()),
      target(new SBTarget()),
      process(new SBProcess()),
      llv8(new LLV8()),
      llscan(new LLScan(llv8.get(), llnode)) {}
LLNodeApi::~LLNodeApi(){};

int LLNodeApi::LoadCore() {
  core_wrap_t* core = llnode->GetCore();
  if (!debugger_initialized) {
    lldb::SBDebugger::Initialize();
    debugger_initialized = true;
  }
  if (core_loaded) {
    return 0;
  }
  *debugger = SBDebugger::Create();
  core_loaded = true;
  *target = debugger->CreateTarget(core->executable);
  if (!target->IsValid()) {
    return 1;
  }
  *process = target->LoadCore(core->core);
  if (!process->IsValid()) {
    return 2;
  }
  // Load V8 constants from postmortem data
  llscan->v8()->Load(*target);
  return 0;
}

std::string LLNodeApi::GetProcessInfo() {
  SBStream info;
  process->GetDescription(info);
  return std::string(info.GetData());
}

uint32_t LLNodeApi::GetProcessID() { return process->GetProcessID(); }

std::string LLNodeApi::GetProcessState() {
  int state = process->GetState();
  switch (state) {
    case StateType::eStateInvalid:
      return "invalid";
    case StateType::eStateUnloaded:
      return "unloaded";
    case StateType::eStateConnected:
      return "connected";
    case StateType::eStateAttaching:
      return "attaching";
    case StateType::eStateLaunching:
      return "launching";
    case StateType::eStateStopped:
      return "stopped";
    case StateType::eStateRunning:
      return "running";
    case StateType::eStateStepping:
      return "stepping";
    case StateType::eStateCrashed:
      return "crashed";
    case StateType::eStateDetached:
      return "detached";
    case StateType::eStateExited:
      return "exited";
    case StateType::eStateSuspended:
      return "suspended";
    default:
      return "unknown";
  }
}

std::string LLNodeApi::GetExecutableName() {
  const char* str = target->GetExecutable().GetFilename();
  std::string res = str;
  return res;
}

std::string LLNodeApi::GetThreadStopReason(size_t thread_index) {
  SBThread thread = process->GetThreadAtIndex(thread_index);
  char buf[100];
  thread.GetStopDescription(buf, 100);
  std::string res = buf;
  return res;
}

uint64_t LLNodeApi::GetThreadID(size_t thread_index) {
  SBThread thread = process->GetThreadAtIndex(thread_index);
  return thread.GetThreadID();
}

std::string LLNodeApi::GetThreadName(size_t thread_index) {
  SBThread thread = process->GetThreadAtIndex(thread_index);
  const char* buf = thread.GetName();
  if (buf == nullptr) return std::string();
  std::string res = buf;
  return res;
}

std::string LLNodeApi::GetThreadStartAddress(size_t thread_index) {
  SBThread thread = process->GetThreadAtIndex(thread_index);
  SBFrame frame = thread.GetFrameAtIndex(0);
  char buf[20];
  snprintf(buf, sizeof(buf), "0x%016" PRIx64, frame.GetPC());
  std::string res = buf;
  return res;
}

uint32_t LLNodeApi::GetThreadCount() { return process->GetNumThreads(); }

uint32_t LLNodeApi::GetFrameCountByThreadId(size_t thread_index) {
  SBThread thread = process->GetThreadAtIndex(thread_index);
  if (!thread.IsValid()) {
    return 0;
  }
  return thread.GetNumFrames();
}

frame_t* LLNodeApi::GetFrameInfo(size_t thread_index, size_t frame_index) {
  long long key = (static_cast<long long>(thread_index) << 32) + frame_index;
  if (frame_map.count(key) != 0) return frame_map.at(key);
  SBThread thread = process->GetThreadAtIndex(thread_index);
  SBFrame frame = thread.GetFrameAtIndex(frame_index);
  SBSymbol symbol = frame.GetSymbol();
  std::string result;
  if (symbol.IsValid()) {
    native_frame_t* nft = new native_frame_t;
    nft->type = kNativeFrame;
    nft->name = "Native";
    nft->function = static_cast<std::string>(frame.GetFunctionName());
    lldb::SBModule module = frame.GetModule();
    lldb::SBFileSpec moduleFileSpec = module.GetFileSpec();
    nft->module_file = static_cast<std::string>(moduleFileSpec.GetDirectory()) +
                       "/" +
                       static_cast<std::string>(moduleFileSpec.GetFilename());
    lldb::SBCompileUnit compileUnit = frame.GetCompileUnit();
    lldb::SBFileSpec compileUnitFileSpec = compileUnit.GetFileSpec();
    if (compileUnitFileSpec.GetDirectory() != nullptr ||
        compileUnitFileSpec.GetFilename() != nullptr) {
      SBLineEntry entry = frame.GetLineEntry();
      nft->compile_unit_file =
          static_cast<std::string>(compileUnitFileSpec.GetDirectory()) + "/" +
          static_cast<std::string>(compileUnitFileSpec.GetFilename()) + ":" +
          std::to_string(entry.GetLine()) + ":" +
          std::to_string(entry.GetColumn());
    }
    frame_map.insert(FrameMap::value_type(key, nft));
    return nft;
  } else {
    // V8 frame
    Error err;
    v8::JSFrame v8_frame(llscan->v8(), static_cast<int64_t>(frame.GetFP()));
    js_frame_t* jft = v8_frame.InspectX(true, err);
#ifdef LLDB_SBMemoryRegionInfoList_h_
    {
      if (jft == nullptr || jft->function.length() == 0) {
        lldb::SBMemoryRegionInfo info;
        const uint64_t pc = frame.GetPC();
        if (target->GetProcess().GetMemoryRegionInfo(pc, info).Success() &&
            info.IsExecutable() && info.IsWritable()) {
          if (jft == nullptr) {
            jft = new js_frame_t;
          }
          jft->function = "<builtin>";
        }
      }
    }
#endif
    if (jft == nullptr) return nullptr;
    jft->type = kJsFrame;
    if (err.Fail() || jft->function.length() == 0 || jft->function[0] == '<') {
      if (jft->function[0] == '<') {
        jft->name = "Unknown";
      } else {
        jft->name = "???";
        jft->function = "???";
      }
    } else {
      // V8 symbol
      jft->name = "JavaScript";
    }
    frame_map.insert(FrameMap::value_type(key, jft));
    return jft;
  }
}

void LLNodeApi::HeapScanMonitorCallBack_(LLNode* llnode, uint32_t now,
                                         uint32_t total) {
  llnode->HeapScanMonitir(now, total);
}

bool LLNodeApi::ScanHeap() {
  SBCommandReturnObject result;
  if (!llscan->ScanHeapForObjects(*target, result, HeapScanMonitorCallBack_)) {
    return false;
  }
  return true;
}

void LLNodeApi::CacheAndSortHeapByCount() {
  object_types_by_count.clear();
  for (const auto& kv : llscan->GetMapsToInstances()) {
    // put TypeRecord*
    object_types_by_count.push_back(kv.second);
  }
  // sort by count
  std::sort(object_types_by_count.begin(), object_types_by_count.end(),
            TypeRecord::CompareInstanceCounts);
}

void LLNodeApi::CacheAndSortHeapBySize() {
  object_types_by_size.clear();
  for (const auto& kv : llscan->GetMapsToInstances()) {
    // put TypeRecord*
    object_types_by_size.push_back(kv.second);
  }
  // sort by size
  std::sort(object_types_by_size.begin(), object_types_by_size.end(),
            TypeRecord::CompareInstanceSizes);
}

uint32_t LLNodeApi::GetHeapTypeCount(int type) {
  bool by_count = type == 1;
  std::vector<TypeRecord*> objet_types =
      by_count ? object_types_by_count : object_types_by_size;
  return objet_types.size();
}

std::string LLNodeApi::GetTypeName(size_t type_index, int type) {
  bool by_count = type == 1;
  std::vector<TypeRecord*> objet_types =
      by_count ? object_types_by_count : object_types_by_size;
  if (objet_types.size() <= type_index) {
    return "";
  }
  return objet_types[type_index]->GetTypeName();
}

uint32_t LLNodeApi::GetTypeInstanceCount(size_t type_index, int type) {
  bool by_count = type == 1;
  std::vector<TypeRecord*> objet_types =
      by_count ? object_types_by_count : object_types_by_size;
  if (objet_types.size() <= type_index) {
    return 0;
  }
  return objet_types[type_index]->GetInstanceCount();
}

uint32_t LLNodeApi::GetTypeTotalSize(size_t type_index, int type) {
  bool by_count = type == 1;
  std::vector<TypeRecord*> objet_types =
      by_count ? object_types_by_count : object_types_by_size;
  if (objet_types.size() <= type_index) {
    return 0;
  }
  return objet_types[type_index]->GetTotalInstanceSize();
}

string** LLNodeApi::GetTypeInstances(size_t type_index, int type) {
  bool by_count = type == 1;
  std::vector<TypeRecord*> objet_types =
      by_count ? object_types_by_count : object_types_by_size;
  std::string key = std::to_string(type) + std::to_string(type_index);
  if (instances_map.count(key) != 0) return instances_map.at(key);
  if (objet_types.size() <= type_index) {
    return nullptr;
  }
  uint32_t length = objet_types[type_index]->GetInstanceCount();
  string** instances = new string*[length];
  std::set<uint64_t> list = objet_types[type_index]->GetInstances();
  uint32_t index = 0;
  for (auto it = list.begin(); it != list.end(); ++it) {
    char buf[20];
    snprintf(buf, sizeof(buf), "0x%016" PRIx64, (*it));
    string* addr = new string(buf);
    if (index < length) instances[index++] = addr;
  }
  instances_map.insert(InstancesMap::value_type(key, instances));
  return instances;
}

std::string LLNodeApi::GetObject(uint64_t address, bool detailed) {
  v8::Value v8_value(llscan->v8(), address);
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = detailed;
  inspect_options.length = 100;
  inspect_options.start_address = address;

  Error err;
  std::string result = v8_value.Inspect(&inspect_options, err);
  if (err.Fail()) {
    return "Failed to get object";
  }
  return result;
}

inspect_t* LLNodeApi::Inspect(uint64_t address, bool detailed,
                              unsigned int current, unsigned int limit) {
  std::string key = std::to_string(address) + std::to_string(detailed) +
                    std::to_string(current) + std::to_string(limit);
  if (inspect_map.count(key) != 0) return inspect_map.at(key);
  v8::Value v8_value(llscan->v8(), address);
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = detailed;
  inspect_options.length = 100;
  inspect_options.start_address = address;
  inspect_options.current = current;
  inspect_options.limit = limit;
  // TDOD: search function source
  // inspect_options.print_source = true;

  Error err;
  inspect_t* result = v8_value.InspectX(&inspect_options, err);
  if (err.Fail() || result == nullptr) return nullptr;
  inspect_map.insert(InspectMap::value_type(key, result));
  return result;
}
}  // namespace llnode
