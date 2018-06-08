#ifndef SRC_COMMON_H
#define SRC_COMMON_H

#include <string>

namespace llnode {
typedef struct {
  std::string symbol;
  std::string function;
  std::string module_file;
  std::string compile_unit_file;
} native_frame_t;

typedef struct {
  std::string function;
  std::string context;
  std::string arguments;
  std::string line;
  std::string address;
} valid_js_frame_t;

typedef struct {
  int type;
  std::string symbol;
  union {
    const char* invalid_js_frame;
    valid_js_frame_t* valid_js_frame;
  };
} js_frame_t;

typedef struct {
  int type;
  union {
    native_frame_t* native_frame;
    js_frame_t* js_frame;
  };
} frame_t;

class LLMonitor {
public:
  explicit LLMonitor();
  ~LLMonitor();
  void SetProgress(double progress);
  double GetProgress();
private:
  double progress;
};
}

#endif
