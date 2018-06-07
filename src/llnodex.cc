#include <nan.h>

namespace llnode {

NAN_MODULE_INIT(InitAll) {
}

NODE_API_MODULE(addon, InitAll)

}  // namespace llnode
