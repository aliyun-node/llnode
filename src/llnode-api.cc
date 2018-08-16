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
using v8::LLV8;
using lldb::SBDebugger;
using lldb::SBTarget;
using lldb::SBProcess;
using lldb::SBStream;
using lldb::SBThread;
using lldb::SBFrame;
using lldb::SBSymbol;
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
  SBThread thread = process->GetThreadAtIndex(thread_index);
  SBFrame frame = thread.GetFrameAtIndex(frame_index);
  SBSymbol symbol = frame.GetSymbol();
  std::string result;
  frame_t* ft = new frame_t;
  if (symbol.IsValid()) {
    ft->type = 1;
    ft->native_frame = new native_frame_t;
    ft->native_frame->symbol = "Native";
    ft->native_frame->function =
      static_cast<std::string>(frame.GetFunctionName());
    lldb::SBModule module = frame.GetModule();
    lldb::SBFileSpec moduleFileSpec = module.GetFileSpec();
    ft->native_frame->module_file =
      static_cast<std::string>(moduleFileSpec.GetDirectory()) + "/" +
      static_cast<std::string>(moduleFileSpec.GetFilename());
    lldb::SBCompileUnit compileUnit = frame.GetCompileUnit();
    lldb::SBFileSpec compileUnitFileSpec = compileUnit.GetFileSpec();
    if (compileUnitFileSpec.GetDirectory() != nullptr ||
        compileUnitFileSpec.GetFilename() != nullptr) {
      ft->native_frame->compile_unit_file =
        static_cast<std::string>(compileUnitFileSpec.GetDirectory()) + "/" +
        static_cast<std::string>(compileUnitFileSpec.GetFilename());
    }
  } else {
    ft->type = 2;
    // V8 frame
    llnode::v8::Error err;
    llnode::v8::JSFrame v8_frame(llscan->v8(),
                                 static_cast<int64_t>(frame.GetFP()));
    ft->js_frame = new js_frame_t;
    v8_frame.InspectX(true, ft->js_frame, err);
    bool invalid = ft->js_frame->type == 1;
    if (err.Fail() || (invalid && strlen(ft->js_frame->invalid_js_frame) == 0)
        || (invalid && ft->js_frame->invalid_js_frame[0] == '<')) {
      if (invalid && ft->js_frame->invalid_js_frame[0] == '<') {
        ft->js_frame->symbol = "Unknown";
      } else {
        ft->js_frame->invalid_js_frame = "???";
      }
    } else {
      // V8 symbol
      ft->js_frame->symbol = "JavaScript";
    }
  }
  return ft;
}
}
