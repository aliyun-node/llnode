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
}
