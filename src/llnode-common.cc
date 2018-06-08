#include "src/llnode-common.h"

namespace llnode {
LLMonitor::LLMonitor(): progress(0) {}

LLMonitor::~LLMonitor() {}

void LLMonitor::SetProgress(double progress) {
  this->progress = progress;
}

double LLMonitor::GetProgress() {
  return progress;
}
}
