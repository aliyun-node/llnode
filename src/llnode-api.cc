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

#include "src/llnode-module.h"
#include "src/llnode-api.h"
#include "src/llscan.h"
#include "src/llv8.h"

namespace llnode {
using std::set;
using std::string;
using v8::LLV8;
using lldb::SBDebugger;
using lldb::SBTarget;
using lldb::SBProcess;
using lldb::SBStream;
using lldb::SBThread;
using lldb::SBFrame;
using lldb::SBSymbol;
using lldb::SBCommandReturnObject;
using lldb::StateType;

LLNodeApi::LLNodeApi(LLNode* llnode)
  : llnode(llnode),
    debugger(new SBDebugger()),
    target(new SBTarget()),
    process(new SBProcess()),
    llv8(new LLV8()),
    llscan(new LLScan(llv8.get())) {}
LLNodeApi::~LLNodeApi() {};

int LLNodeApi::LoadCore() {
  core_wrap_t* core = llnode->GetCore();
  if(!debugger_initialized) {
    lldb::SBDebugger::Initialize();
    debugger_initialized = true;
  }
  if(core_loaded) {
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

uint32_t LLNodeApi::GetProcessID() {
  return process->GetProcessID();
}

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

uint32_t LLNodeApi::GetThreadCount() {
  return process->GetNumThreads();
}

uint32_t LLNodeApi::GetFrameCountByThreadId(size_t thread_index) {
  SBThread thread = process->GetThreadAtIndex(thread_index);
  if (!thread.IsValid()) {
    return 0;
  }
  return thread.GetNumFrames();
}

frame_t* LLNodeApi::GetFrameInfo(size_t thread_index, size_t frame_index) {
  long long key = (static_cast<long long>(thread_index) << 32) + frame_index;
  if(frame_map.count(key) != 0) {
    return frame_map.at(key);
  }
  SBThread thread = process->GetThreadAtIndex(thread_index);
  SBFrame frame = thread.GetFrameAtIndex(frame_index);
  SBSymbol symbol = frame.GetSymbol();
  std::string result;
  if (symbol.IsValid()) {
    native_frame_t* nft = new native_frame_t;
    nft->type = kNativeFrame;
    nft->name = "Native";
    nft->function =
      static_cast<std::string>(frame.GetFunctionName());
    lldb::SBModule module = frame.GetModule();
    lldb::SBFileSpec moduleFileSpec = module.GetFileSpec();
    nft->module_file =
      static_cast<std::string>(moduleFileSpec.GetDirectory()) + "/" +
      static_cast<std::string>(moduleFileSpec.GetFilename());
    lldb::SBCompileUnit compileUnit = frame.GetCompileUnit();
    lldb::SBFileSpec compileUnitFileSpec = compileUnit.GetFileSpec();
    if (compileUnitFileSpec.GetDirectory() != nullptr ||
        compileUnitFileSpec.GetFilename() != nullptr) {
      nft->compile_unit_file =
        static_cast<std::string>(compileUnitFileSpec.GetDirectory()) + "/" +
        static_cast<std::string>(compileUnitFileSpec.GetFilename());
    }
    frame_map.insert(FrameMap::value_type(key, nft));
    return nft;
  } else {
    // V8 frame
    v8::Error err;
    v8::JSFrame v8_frame(llscan->v8(),
                         static_cast<int64_t>(frame.GetFP()));
    js_frame_t* jft = v8_frame.InspectX(true, err);
    if(jft == nullptr) return nullptr;
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

bool LLNodeApi::ScanHeap() {
  SBCommandReturnObject result;
  if (!llscan->ScanHeapForObjects(*target, result)) {
    return false;
  }
  return true;
}

void LLNodeApi::CacheAndSortHeap() {
  object_types.clear();
  for (const auto& kv : llscan->GetMapsToInstances()) {
    // put TypeRecord*
    object_types.push_back(kv.second);
  }
  // sort by count
  std::sort(object_types.begin(), object_types.end(),
            TypeRecord::CompareInstanceCounts);
}

uint32_t LLNodeApi::GetHeapTypeCount() {
  return object_types.size();
}

std::string LLNodeApi::GetTypeName(size_t type_index) {
  if (object_types.size() <= type_index) {
    return "";
  }
  return object_types[type_index]->GetTypeName();
}

uint32_t LLNodeApi::GetTypeInstanceCount(size_t type_index) {
  if (object_types.size() <= type_index) {
    return 0;
  }
  return object_types[type_index]->GetInstanceCount();
}

uint32_t LLNodeApi::GetTypeTotalSize(size_t type_index) {
  if (object_types.size() <= type_index) {
    return 0;
  }
  return object_types[type_index]->GetTotalInstanceSize();
}

string** LLNodeApi::GetTypeInstances(size_t type_index) {
  if(instances_map.count(type_index) != 0) {
    return instances_map.at(type_index);
  }
  if (object_types.size() <= type_index) {
    return nullptr;
  }
  uint32_t length = object_types[type_index]->GetInstanceCount();
  string** instances = new string*[length];
  std::set<uint64_t> list = object_types[type_index]->GetInstances();
  uint32_t index = 0;
  for(auto it = list.begin(); it != list.end(); ++it) {
    char buf[20];
    snprintf(buf, sizeof(buf), "0x%016" PRIx64, (*it));
    string* addr = new string(buf);
    if(index < length)
      instances[index++] = addr;
  }
  instances_map.insert(InstancesMap::value_type(type_index, instances));
  return instances;
}

std::string LLNodeApi::GetObject(uint64_t address, bool detailed) {
  v8::Value v8_value(llscan->v8(), address);
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = detailed;
  inspect_options.length = 100;
  inspect_options.start_address = address;

  v8::Error err;
  std::string result = v8_value.Inspect(&inspect_options, err);
  if (err.Fail()) {
    return "Failed to get object";
  }
  return result;
}

inspect_t* LLNodeApi::Inspect(uint64_t address, bool detailed) {
  v8::Value v8_value(llscan->v8(), address);
  v8::Value::InspectOptions inspect_options;
  inspect_options.detailed = detailed;
  inspect_options.length = 100;
  inspect_options.start_address = address;
  // TDOD: search function source
  // inspect_options.print_source = true;

  v8::Error err;
  inspect_t* result = v8_value.InspectX(&inspect_options, err);
  if (err.Fail() || result == nullptr) return nullptr;
  return result;
}
}
