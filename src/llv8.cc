#include <assert.h>

#include <algorithm>
#include <cinttypes>
#include <cstdarg>

#include "llnode-api.h"
#include "llv8-inl.h"
#include "llv8.h"

namespace llnode {
namespace v8 {

using lldb::addr_t;
using lldb::SBError;
using lldb::SBTarget;

static std::string kConstantPrefix = "v8dbg_";

void LLV8::Load(SBTarget target) {
  // Reload process anyway
  process_ = target.GetProcess();

  // No need to reload
  if (target_ == target) return;

  target_ = target;

  common.Assign(target);
  smi.Assign(target, &common);
  heap_obj.Assign(target, &common);
  map.Assign(target, &common);
  js_object.Assign(target, &common);
  heap_number.Assign(target, &common);
  js_array.Assign(target, &common);
  js_function.Assign(target, &common);
  shared_info.Assign(target, &common);
  code.Assign(target, &common);
  scope_info.Assign(target, &common);
  context.Assign(target, &common);
  script.Assign(target, &common);
  string.Assign(target, &common);
  one_byte_string.Assign(target, &common);
  two_byte_string.Assign(target, &common);
  cons_string.Assign(target, &common);
  sliced_string.Assign(target, &common);
  thin_string.Assign(target, &common);
  fixed_array_base.Assign(target, &common);
  fixed_array.Assign(target, &common);
  fixed_typed_array_base.Assign(target, &common);
  oddball.Assign(target, &common);
  js_array_buffer.Assign(target, &common);
  js_array_buffer_view.Assign(target, &common);
  js_regexp.Assign(target, &common);
  js_date.Assign(target, &common);
  descriptor_array.Assign(target, &common);
  name_dictionary.Assign(target, &common);
  frame.Assign(target, &common);
  symbol.Assign(target, &common);
  types.Assign(target, &common);
}

int64_t LLV8::LoadPtr(int64_t addr, Error& err) {
  SBError sberr;
  int64_t value =
      process_.ReadPointerFromMemory(static_cast<addr_t>(addr), sberr);
  if (sberr.Fail()) {
    // TODO(joyeecheung): use Error::Failure() to report information when
    // there is less noise from here.
    err = Error(true, "Failed to load pointer from v8 memory");
    return -1;
  }

  err = Error::Ok();
  return value;
}


int64_t LLV8::LoadUnsigned(int64_t addr, uint32_t byte_size, Error& err) {
  SBError sberr;
  int64_t value = process_.ReadUnsignedFromMemory(static_cast<addr_t>(addr),
                                                  byte_size, sberr);

  if (sberr.Fail()) {
    // TODO(joyeecheung): use Error::Failure() to report information when
    // there is less noise from here.
    err = Error(true, "Failed to load unsigned from v8 memory");
    return -1;
  }

  err = Error::Ok();
  return value;
}


double LLV8::LoadDouble(int64_t addr, Error& err) {
  SBError sberr;
  int64_t value = process_.ReadUnsignedFromMemory(static_cast<addr_t>(addr),
                                                  sizeof(double), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load double from v8 memory, "
        "addr=0x%016" PRIx64,
        addr);
    return -1.0;
  }

  err = Error::Ok();
  return *reinterpret_cast<double*>(&value);
}


std::string LLV8::LoadBytes(int64_t addr, int64_t length, Error& err) {
  uint8_t* buf = new uint8_t[length + 1];
  SBError sberr;
  process_.ReadMemory(addr, buf, static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load v8 backing store memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return std::string();
  }

  std::string res;
  char tmp[10];
  for (int i = 0; i < length; ++i) {
    snprintf(tmp, sizeof(tmp), "%s%02x", (i == 0 ? "" : ", "), buf[i]);
    res += tmp;
  }
  delete[] buf;
  return res;
}

std::string* LLV8::LoadBytesX(int64_t addr, int64_t length, int64_t start,
                              int64_t end, Error& err) {
  uint8_t* buf = new uint8_t[length + 1];
  SBError sberr;
  process_.ReadMemory(addr, buf, static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load v8 backing store memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return nullptr;
  }

  std::string* list = new std::string[end - start];
  char tmp[10];
  for (int64_t i = start; i < end; ++i) {
    snprintf(tmp, sizeof(tmp), "%02x", buf[i]);
    list[i - start] = tmp;
  }
  delete[] buf;
  return list;
}

std::string LLV8::LoadString(int64_t addr, int64_t length, Error& err) {
  if (length < 0) {
    err = Error::Failure("Failed to load V8 one byte string - Invalid length");
    return std::string();
  }

  char* buf = new char[length + 1];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
                      static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load v8 one byte string memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return std::string();
  }

  buf[length] = '\0';

  std::string res = buf;
  delete[] buf;
  err = Error::Ok();
  return res;
}

std::string LLV8::Utf16ToUtf8(const std::u16string& u16_str) {
  if (u16_str.empty()) {
    return std::string();
  }
  const char16_t* p = u16_str.data();
  std::u16string::size_type len = u16_str.length();
  if (p[0] == 0xFEFF) {
    // bom
    p += 1;
    len -= 1;
  }

  std::string u8_str;
  u8_str.reserve(len * 3);

  char16_t u16_char;
  for (std::u16string::size_type i = 0; i < len; ++i) {
    u16_char = p[i];
    if (u16_char < 0x0080) {
      u8_str.push_back((char)(u16_char & 0x00FF));
      continue;
    }
    if (u16_char >= 0x0080 && u16_char <= 0x07FF) {
      u8_str.push_back((char)(((u16_char >> 6) & 0x1F) | 0xC0));
      u8_str.push_back((char)((u16_char & 0x3F) | 0x80));
      continue;
    }
    if (u16_char >= 0xD800 && u16_char <= 0xDBFF) {
      uint32_t highSur = u16_char;
      uint32_t lowSur = p[++i];
      uint32_t codePoint = highSur - 0xD800;
      codePoint <<= 10;
      codePoint |= lowSur - 0xDC00;
      codePoint += 0x10000;
      u8_str.push_back((char)((codePoint >> 18) | 0xF0));
      u8_str.push_back((char)(((codePoint >> 12) & 0x3F) | 0x80));
      u8_str.push_back((char)(((codePoint >> 06) & 0x3F) | 0x80));
      u8_str.push_back((char)((codePoint & 0x3F) | 0x80));
      continue;
    }
    {
      u8_str.push_back((char)(((u16_char >> 12) & 0x0F) | 0xE0));
      u8_str.push_back((char)(((u16_char >> 6) & 0x3F) | 0x80));
      u8_str.push_back((char)((u16_char & 0x3F) | 0x80));
      continue;
    }
  }

  return u8_str;
}

std::string LLV8::LoadTwoByteString(int64_t addr, int64_t length, Error& err,
                                    bool utf16) {
  if (length < 0) {
    err = Error::Failure("Failed to load V8 two byte string - Invalid length");
    return std::string();
  }

  // get source code / debug line needs origin utf16 string, although it
  // will display wrong when the string contains characters that require
  // two bytes to represented
  if (utf16) {
    char* buf = new char[2 * length + 1];
    SBError sberr;
    process_.ReadMemory(static_cast<addr_t>(addr), buf,
                        static_cast<size_t>(length * 2), sberr);
    if (sberr.Fail()) {
      err = Error::Failure(
          "Failed to load V8 two byte string memory, "
          "addr=0x%016" PRIx64 ", length=%" PRId64,
          addr, length);
      delete[] buf;
      return std::string();
    }
    for (int64_t i = 0; i < length; i++) buf[i] = buf[i * 2];
    buf[length] = '\0';
    std::string res = buf;
    delete[] buf;
    err = Error::Ok();
    return res;
  }

  char16_t* buf = new char16_t[length];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
                      static_cast<size_t>(length * 2), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load V8 two byte string memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return std::string();
  }
  std::string res = Utf16ToUtf8(buf);
  delete[] buf;
  if (err.Fail()) {
    return std::string();
  }
  err = Error::Ok();
  return res;
}


uint8_t* LLV8::LoadChunk(int64_t addr, int64_t length, Error& err) {
  uint8_t* buf = new uint8_t[length];
  SBError sberr;
  process_.ReadMemory(static_cast<addr_t>(addr), buf,
                      static_cast<size_t>(length), sberr);
  if (sberr.Fail()) {
    err = Error::Failure(
        "Failed to load V8 chunk memory, "
        "addr=0x%016" PRIx64 ", length=%" PRId64,
        addr, length);
    delete[] buf;
    return nullptr;
  }

  err = Error::Ok();
  return buf;
}


// reset_line - make line_start absolute vs start of function
//            otherwise relative to last end
// returns line cursor
uint32_t JSFrame::GetSourceForDisplay(bool reset_line, uint32_t line_start,
                                      uint32_t line_limit, std::string lines[],
                                      uint32_t& lines_found, Error& err) {
  v8::JSFunction fn = GetFunction(err);
  if (err.Fail()) {
    return line_start;
  }

  v8::SharedFunctionInfo info = fn.Info(err);
  if (err.Fail()) {
    return line_start;
  }

  v8::Script script = info.GetScript(err);
  if (err.Fail()) {
    return line_start;
  }

  // Check if this is being run multiple times against the same frame
  // if not, reset last line
  if (reset_line) {
    uint32_t pos = info.StartPosition(err);
    if (err.Fail()) {
      return line_start;
    }
    int64_t tmp_line;
    int64_t tmp_col;
    script.GetLineColumnFromPos(pos, tmp_line, tmp_col, err);
    if (err.Fail()) {
      return line_start;
    }
    line_start += tmp_line;
  }

  script.GetLines(line_start, lines, line_limit, lines_found, err);
  if (err.Fail()) {
    const char* msg = err.GetMessage();
    if (msg == nullptr) {
      err = Error::Failure("Failed to get Function Source");
    }
    return line_start;
  }
  return line_start + lines_found;
}


// On 64 bits systems, V8 stores SMIs (small ints) in the top 32 bits of
// a 64 bits word.  Frame markers used to obey this convention but as of
// V8 5.8, they are stored as 32 bits SMIs with the top half set to zero.
// Shift the raw value up to make it a normal SMI again.
Smi JSFrame::FromFrameMarker(Value value) const {
  if (v8()->smi()->kShiftSize == 31 && Smi(value).Check() &&
      value.raw() < 1LL << 31) {
    value = Value(v8(), value.raw() << 31);
  }
  return Smi(value);
}


std::string JSFrame::Inspect(bool with_args, Error& err) {
  Value context =
      v8()->LoadValue<Value>(raw() + v8()->frame()->kContextOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_context = FromFrameMarker(context);
  if (smi_context.Check() &&
      smi_context.GetValue() == v8()->frame()->kAdaptorFrame) {
    return "<adaptor>";
  }

  Value marker =
      v8()->LoadValue<Value>(raw() + v8()->frame()->kMarkerOffset, err);
  if (err.Fail()) return std::string();

  Smi smi_marker = FromFrameMarker(marker);
  if (smi_marker.Check()) {
    int64_t value = smi_marker.GetValue();
    if (value == v8()->frame()->kEntryFrame) {
      return "<entry>";
    } else if (value == v8()->frame()->kEntryConstructFrame) {
      return "<entry_construct>";
    } else if (value == v8()->frame()->kExitFrame) {
      return "<exit>";
    } else if (value == v8()->frame()->kInternalFrame) {
      return "<internal>";
    } else if (value == v8()->frame()->kConstructFrame) {
      return "<constructor>";
    } else if (value == v8()->frame()->kStubFrame) {
      return "<stub>";
    } else if (value != v8()->frame()->kJSFrame &&
               value != v8()->frame()->kOptimizedFrame) {
      err = Error::Failure("Unknown frame marker %" PRId64, value);
      return std::string();
    }
  }

  // We are dealing with function or internal code (probably stub)
  JSFunction fn = GetFunction(err);
  if (err.Fail()) return std::string();

  int64_t fn_type = fn.GetType(err);
  if (err.Fail()) return std::string();

  if (fn_type == v8()->types()->kCodeType) return "<internal code>";
  if (fn_type != v8()->types()->kJSFunctionType) return "<non-function>";

  std::string args;
  if (with_args) {
    args = InspectArgs(fn, err);
    if (err.Fail()) return std::string();
  }

  char tmp[128];
  snprintf(tmp, sizeof(tmp), " fn=0x%016" PRIx64, fn.raw());
  return fn.GetDebugLine(args, err) + tmp;
}

js_frame_t* JSFrame::InspectX(bool with_args, Error& err) {
  Value context =
      v8()->LoadValue<Value>(raw() + v8()->frame()->kContextOffset, err);
  if (err.Fail()) return nullptr;

  js_frame_t* jft = new js_frame_t;
  Smi smi_context = FromFrameMarker(context);
  if (smi_context.Check() &&
      smi_context.GetValue() == v8()->frame()->kAdaptorFrame) {
    jft->function = "<adaptor>";
    return jft;
  }

  Value marker =
      v8()->LoadValue<Value>(raw() + v8()->frame()->kMarkerOffset, err);
  if (err.Fail()) {
    delete jft;
    return nullptr;
  }

  Smi smi_marker = FromFrameMarker(marker);
  if (smi_marker.Check()) {
    int64_t value = smi_marker.GetValue();
    if (value == v8()->frame()->kEntryFrame) {
      jft->function = "<entry>";
      return jft;
    } else if (value == v8()->frame()->kEntryConstructFrame) {
      jft->function = "<entry_construct>";
      return jft;
    } else if (value == v8()->frame()->kExitFrame) {
      jft->function = "<exit>";
      return jft;
    } else if (value == v8()->frame()->kInternalFrame) {
      jft->function = "<internal>";
      return jft;
    } else if (value == v8()->frame()->kConstructFrame) {
      jft->function = "<constructor>";
      return jft;
    } else if (value == v8()->frame()->kStubFrame) {
      jft->function = "<stub>";
      return jft;
    } else if (value != v8()->frame()->kJSFrame &&
               value != v8()->frame()->kOptimizedFrame) {
      err = Error::Failure("Unknown frame marker %" PRId64, value);
      jft->function = "";
      return jft;
    }
  }

  // We are dealing with function or internal code (probably stub)
  JSFunction fn = GetFunction(err);
  if (err.Fail()) {
    delete jft;
    return nullptr;
  }

  int64_t fn_type = fn.GetType(err);
  if (err.Fail()) {
    delete jft;
    return nullptr;
  }

  if (fn_type == v8()->types()->kCodeType) {
    jft->function = "<internal code>";
    return jft;
  };
  if (fn_type != v8()->types()->kJSFunctionType) {
    jft->function = "<non-function>";
    return jft;
  };

  if (with_args) {
    jft->args = InspectArgsX(fn, err);
    if (err.Fail()) {
      delete jft;
      return nullptr;
    }
  }

  char tmp[128];
  snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, fn.raw());
  jft->address = tmp;
  jft->debug = fn.GetDebugLineX(err);
  if (err.Fail()) {
    delete jft;
    return nullptr;
  }
  jft->function = jft->debug->func_name;
  return jft;
}


std::string JSFrame::InspectArgs(JSFunction fn, Error& err) {
  SharedFunctionInfo info = fn.Info(err);
  if (err.Fail()) return std::string();

  int64_t param_count = info.ParameterCount(err);
  if (err.Fail()) return std::string();

  Value receiver = GetReceiver(param_count, err);
  if (err.Fail()) return std::string();

  InspectOptions options;

  std::string res = "this=" + receiver.Inspect(&options, err);
  if (err.Fail()) return std::string();

  for (int64_t i = 0; i < param_count; i++) {
    Value param = GetParam(i, param_count, err);
    if (err.Fail()) return std::string();

    res += ", " + param.Inspect(&options, err);
    if (err.Fail()) return std::string();
  }

  return res;
}

args_t* JSFrame::InspectArgsX(JSFunction fn, Error& err) {
  SharedFunctionInfo info = fn.Info(err);
  if (err.Fail()) return nullptr;

  int64_t param_count = info.ParameterCount(err);
  if (err.Fail()) return nullptr;

  Value receiver = GetReceiver(param_count, err);
  if (err.Fail()) return nullptr;

  InspectOptions options;

  args_t* args = new args_t;
  args->context = receiver.InspectX(&options, err);
  if (err.Fail()) {
    delete args;
    return nullptr;
  }

  args->length = param_count;
  try {
    args->args_list = new inspect_t*[param_count];
  } catch (std::bad_alloc) {
    delete args;
    return nullptr;
  }
  for (int64_t i = 0; i < param_count; i++) {
    Value param = GetParam(i, param_count, err);
    if (err.Fail()) {
      delete args;
      return nullptr;
    }

    args->args_list[i] = param.InspectX(&options, err);
    if (err.Fail()) {
      delete args;
      return nullptr;
    }
  }

  return args;
}


std::string JSFunction::GetDebugLine(std::string args, Error& err) {
  SharedFunctionInfo info = Info(err);
  if (err.Fail()) return std::string();

  std::string res = info.ProperName(err);
  if (err.Fail()) return std::string();

  if (!args.empty()) res += "(" + args + ")";

  res += " at ";

  std::string shared;

  res += info.GetPostfix(err);
  if (err.Fail()) return std::string();

  return res;
}

js_function_debug_t* JSFunction::GetDebugLineX(Error& err) {
  SharedFunctionInfo info = Info(err);
  if (err.Fail()) return nullptr;

  js_function_debug_t* js_function_debug = new js_function_debug_t;
  js_function_debug->func_name = info.ProperName(err);
  if (err.Fail()) {
    delete js_function_debug;
    return nullptr;
  }

  js_function_debug->line = info.GetPostfix(err);
  if (err.Fail()) {
    delete js_function_debug;
    return nullptr;
  }

  return js_function_debug;
}

std::string JSFunction::Inspect(InspectOptions* options, Error& err) {
  std::string res = "<function: " + GetDebugLine(std::string(), err);
  if (err.Fail()) return std::string();

  if (options->detailed) {
    HeapObject context_obj = GetContext(err);
    if (err.Fail()) return std::string();

    Context context(context_obj);

    char tmp[128];
    snprintf(tmp, sizeof(tmp), "\n  context=0x%016" PRIx64, context.raw());
    res += tmp;

    {
      InspectOptions ctx_options;
      ctx_options.detailed = true;
      ctx_options.indent_depth = options->indent_depth + 1;
      std::string context_str = context.Inspect(&ctx_options, err);
      if (err.Fail()) return std::string();

      if (!context_str.empty()) res += ":" + context_str;
    }

    if (options->print_source) {
      SharedFunctionInfo info = Info(err);
      if (err.Fail()) return res;

      std::string name_str = info.ProperName(err);
      if (err.Fail()) return res;

      std::string source = GetSource(err);
      if (!err.Fail()) {
        res += "\n  source:\n";
        // name_str may be an empty string but that will match
        // the syntax for an anonymous function declaration correctly.
        res += "function " + name_str;
        res += source + "\n";
      }
    }
  }

  return res + ">";
}

js_function_t* JSFunction::InspectX(InspectOptions* options, Error& err) {
  js_function_t* js_function = new js_function_t;
  js_function->type = kJsFunction;
  js_function->name = "Function";
  js_function_debug_t* debug_info = GetDebugLineX(err);
  if (err.Fail()) {
    delete js_function;
    return nullptr;
  }
  js_function->func_name = debug_info->func_name;
  js_function->debug_line = debug_info->line;
  delete debug_info;

  if (options->detailed) {
    HeapObject context_obj = GetContext(err);
    if (err.Fail()) {
      delete js_function;
      return nullptr;
    }

    Context context(context_obj);

    char tmp[128];
    snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, context.raw());
    js_function->context_address = tmp;
    {
      InspectOptions ctx_options;
      ctx_options.detailed = true;
      ctx_options.indent_depth = options->indent_depth + 1;
      js_function->context = context.InspectX(&ctx_options, err);
      if (err.Fail()) {
        delete js_function;
        return nullptr;
      }
    }

    if (options->print_source) {
      std::string source = GetSource(err);
      if (!err.Fail()) {
        js_function->func_source = source;
      }
    }
  }

  return js_function;
}


std::string JSFunction::GetSource(Error& err) {
  v8::SharedFunctionInfo info = Info(err);
  if (err.Fail()) {
    return std::string();
  }

  v8::Script script = info.GetScript(err);
  if (err.Fail()) {
    return std::string();
  }

  // There is no `Script` for functions created in C++ (and possibly others)
  int64_t type = script.GetType(err);
  if (err.Fail()) {
    return std::string();
  }

  if (type != v8()->types()->kScriptType) {
    return std::string();
  }

  HeapObject source = script.Source(err);
  if (err.Fail()) return std::string();

  int64_t source_type = source.GetType(err);
  if (err.Fail()) return std::string();

  // No source
  if (source_type > v8()->types()->kFirstNonstringType) {
    err = Error::Failure("No source, source_type=%" PRId64, source_type);
    return std::string();
  }

  String str(source);
  std::string source_str = str.ToString(err);

  int64_t start_pos = info.StartPosition(err);

  if (err.Fail()) {
    return std::string();
  }

  int64_t end_pos = info.EndPosition(err);

  if (err.Fail()) {
    return std::string();
  }

  int64_t source_len = source_str.length();

  if (end_pos > source_len) {
    end_pos = source_len;
  }
  int64_t len = end_pos - start_pos;

  std::string res = source_str.substr(start_pos, len);

  return res;
}


std::string JSRegExp::Inspect(InspectOptions* options, Error& err) {
  if (v8()->js_regexp()->kSourceOffset == -1)
    return JSObject::Inspect(options, err);

  std::string res = "<JSRegExp ";

  String src = GetSource(err);
  if (err.Fail()) return std::string();
  res += "source=/" + src.ToString(err) + "/";
  if (err.Fail()) return std::string();

  // Print properties in detailed mode
  if (options->detailed) {
    res += " " + InspectProperties(err);
    if (err.Fail()) return std::string();
  }

  res += ">";
  return res;
}

inspect_t* JSRegExp::InspectX(InspectOptions* options, Error& err) {
  if (v8()->js_regexp()->kSourceOffset == -1)
    return JSObject::InspectX(options, err);

  js_regexp_t* js_regexp = new js_regexp_t;
  js_regexp->type = kJsRegExp;
  js_regexp->name = "JSRegExp";

  String src = GetSource(err);
  if (err.Fail()) {
    delete js_regexp;
    return nullptr;
  }
  js_regexp->source += ("/" + src.ToString(err) + "/");
  if (err.Fail()) {
    delete js_regexp;
    return nullptr;
  }

  // Print properties in detailed mode
  if (options->detailed) {
    // add elements
    js_regexp->elements = InspectElementsX(err);
    if (err.Fail()) {
      delete js_regexp;
      return nullptr;
    }

    // add properties
    HeapObject map_obj = GetMap(err);
    if (err.Fail()) {
      delete js_regexp;
      return nullptr;
    }
    Map map(map_obj);
    bool is_dict = map.IsDictionary(err);
    if (err.Fail()) {
      delete js_regexp;
      return nullptr;
    }
    if (is_dict)
      js_regexp->properties = InspectDictionaryX(err);
    else
      js_regexp->properties = InspectDescriptorsX(map, err);
    if (err.Fail()) {
      delete js_regexp;
      return nullptr;
    }
  }
  return js_regexp;
}


std::string JSDate::Inspect(Error& err) {
  std::string pre = "<JSDate: ";

  Value val = GetValue(err);

  Smi smi(val);
  if (smi.Check()) {
    std::string s = smi.ToString(err);
    if (err.Fail()) {
      return pre + ">";
    }

    return pre + s + ">";
  }

  HeapNumber hn(val);
  if (hn.Check()) {
    std::string s = hn.ToString(true, err);
    if (err.Fail()) {
      return pre + ">";
    }
    return pre + s + ">";
  }

  double d = static_cast<double>(val.raw());
  char buf[128];
  snprintf(buf, sizeof(buf), "%f", d);

  return pre + ">";
}

js_date_t* JSDate::InspectX(Error& err) {
  js_date_t* date = new js_date_t;
  date->type = kJsDate;
  date->name = "JSDate";

  Value val = GetValue(err);

  Smi smi(val);
  if (smi.Check()) {
    std::string s = smi.ToString(err);
    if (err.Fail()) return date;
    date->value = s;
    return date;
  }

  HeapNumber hn(val);
  if (hn.Check()) {
    std::string s = hn.ToString(true, err);
    if (err.Fail()) return date;
    date->value = s;
    return date;
  }

  double d = static_cast<double>(val.raw());
  char buf[128];
  snprintf(buf, sizeof(buf), "%f", d);
  date->value = d;
  return date;
}


std::string SharedFunctionInfo::ProperName(Error& err) {
  String name = Name(err);
  if (err.Fail()) return std::string();

  std::string res = name.ToString(err);
  if (err.Fail() || res.empty()) {
    Value inferred = InferredName(err);
    if (err.Fail()) return std::string();

    // Function may not have inferred name
    if (!inferred.IsHoleOrUndefined(err) && !err.Fail())
      res = inferred.ToString(err);
    if (err.Fail()) return std::string();
  }

  if (res.empty()) res = "(anonymous)";

  return res;
}


std::string SharedFunctionInfo::GetPostfix(Error& err) {
  Script script = GetScript(err);
  if (err.Fail()) return std::string();

  // There is no `Script` for functions created in C++ (and possibly others)
  int64_t type = script.GetType(err);
  if (err.Fail() || type != v8()->types()->kScriptType)
    return std::string("[native code]");

  String name = script.Name(err);
  if (err.Fail()) return std::string();

  int64_t start_pos = StartPosition(err);
  if (err.Fail()) return std::string();

  std::string res = name.ToString(err);
  if (res.empty()) res = "[native code]";

  int64_t line = 0;
  int64_t column = 0;
  script.GetLineColumnFromPos(start_pos, line, column, err);
  if (err.Fail()) return std::string();

  // NOTE: lines start from 1 in most of editors
  char tmp[128];
  snprintf(tmp, sizeof(tmp), ":%d:%d", static_cast<int>(line + 1),
           static_cast<int>(column));
  return res + tmp;
}

std::string SharedFunctionInfo::ToString(Error& err) {
  std::string res = ProperName(err);
  if (err.Fail()) return std::string();

  return res + " at " + GetPostfix(err);
}


// return end_char+1, which may be less than line_limit if source
// ends before end_inclusive
void Script::GetLines(uint64_t start_line, std::string lines[],
                      uint64_t line_limit, uint32_t& lines_found, Error& err) {
  lines_found = 0;

  HeapObject source = Source(err);
  if (err.Fail()) return;

  int64_t type = source.GetType(err);
  if (err.Fail()) return;

  // No source
  if (type > v8()->types()->kFirstNonstringType) {
    err = Error::Failure("No source, source_type=%" PRId64, type);
    return;
  }

  String str(source);
  std::string source_str = str.ToString(err);
  uint64_t limit = source_str.length();

  uint64_t length = 0;
  uint64_t line_i = 0;
  uint64_t i = 0;
  for (; i < limit && lines_found < line_limit; i++) {
    // \r\n should ski adding a line and column on \r
    if (source_str[i] == '\r' && i < limit - 1 && source_str[i + 1] == '\n') {
      i++;
    }
    if (source_str[i] == '\n' || source_str[i] == '\r') {
      if (line_i >= start_line) {
        lines[lines_found] = std::string(source_str, i - length, length);
        lines_found++;
      }
      line_i++;
      length = 0;
    } else {
      length++;
    }
  }
  if (line_i >= start_line && length != 0 && lines_found < line_limit) {
    lines[lines_found] = std::string(source_str, limit - length, length);
    lines_found++;
  }
}


void Script::GetLineColumnFromPos(int64_t pos, int64_t& line, int64_t& column,
                                  Error& err) {
  line = 0;
  column = 0;

  HeapObject source = Source(err);
  if (err.Fail()) return;

  int64_t type = source.GetType(err);
  if (err.Fail()) return;

  // No source
  if (type > v8()->types()->kFirstNonstringType) {
    err = Error(true, "No source");
    return;
  }

  String str(source);
  std::string source_str = str.ToString(err);
  int64_t limit = source_str.length();
  if (limit > pos) limit = pos;

  for (int64_t i = 0; i < limit; i++, column++) {
    // \r\n should ski adding a line and column on \r
    if (source_str[i] == '\r' && i < limit - 1 && source_str[i + 1] == '\n') {
      i++;
    }
    if (source_str[i] == '\n' || source_str[i] == '\r') {
      column = 0;
      line++;
    }
  }
}

bool Value::IsHoleOrUndefined(Error& err) {
  HeapObject obj(this);
  if (!obj.Check()) return false;

  int64_t type = obj.GetType(err);
  if (err.Fail()) return false;

  if (type != v8()->types()->kOddballType) return false;

  Oddball odd(this);
  return odd.IsHoleOrUndefined(err);
}


// TODO(indutny): deduplicate this?
bool Value::IsHole(Error& err) {
  HeapObject obj(this);
  if (!obj.Check()) return false;

  int64_t type = obj.GetType(err);
  if (err.Fail()) return false;

  if (type != v8()->types()->kOddballType) return false;

  Oddball odd(this);
  return odd.IsHole(err);
}


std::string Value::Inspect(InspectOptions* options, Error& err) {
  Smi smi(this);
  if (smi.Check()) return smi.Inspect(err);

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  return obj.Inspect(options, err);
}

inspect_t* Value::InspectX(InspectOptions* options, Error& err) {
  Smi smi(this);
  if (smi.Check()) return smi.InspectX(err);

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return nullptr;
  }

  return obj.InspectX(options, err);
}


std::string Value::GetTypeName(Error& err) {
  Smi smi(this);
  if (smi.Check()) return "(Smi)";

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  return obj.GetTypeName(err);
}


std::string Value::ToString(Error& err) {
  Smi smi(this);
  if (smi.Check()) return smi.ToString(err);

  HeapObject obj(this);
  if (!obj.Check()) {
    err = Error::Failure("Not object and not smi");
    return std::string();
  }

  return obj.ToString(err);
}


std::string HeapObject::ToString(Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return std::string();

  if (type == v8()->types()->kHeapNumberType) {
    HeapNumber n(this);
    return n.ToString(false, err);
  }

  if (type < v8()->types()->kFirstNonstringType) {
    String str(this);
    return str.ToString(err);
  }

  if (type == v8()->types()->kSymbolType) {
    Symbol symbol(this);
    return symbol.ToString(err);
  }

  return "[non-string]";
}


std::string HeapObject::Inspect(InspectOptions* options, Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return std::string();

  // TODO(indutny): make this configurable
  char buf[64];
  if (options->print_map) {
    HeapObject map = GetMap(err);
    if (err.Fail()) return std::string();
    if (options->start_address != raw()) {
      snprintf(buf, sizeof(buf), "(map=0x%016" PRIx64 "):", map.raw());
    } else {
      snprintf(buf, sizeof(buf),
               "0x%016" PRIx64 "(map=0x%016" PRIx64 "):", raw(), map.raw());
    }
  } else if (options->start_address != raw()) {
    snprintf(buf, sizeof(buf), "0x%016" PRIx64 ":", raw());
  } else {
    snprintf(buf, sizeof(buf), "");
  }
  std::string pre = buf;

  if (type == v8()->types()->kGlobalObjectType) return pre + "<Global>";
  if (type == v8()->types()->kGlobalProxyType) return pre + "<Global proxy>";
  if (type == v8()->types()->kCodeType) return pre + "<Code>";
  if (type == v8()->types()->kMapType) {
    Map m(this);
    return pre + m.Inspect(options, err);
  }

  if (IsJSErrorType(err)) {
    JSError error(this);
    return pre + error.Inspect(options, err);
  }

  if (JSObject::IsObjectType(v8(), type)) {
    JSObject o(this);
    return pre + o.Inspect(options, err);
  }

  if (type == v8()->types()->kHeapNumberType) {
    HeapNumber n(this);
    return pre + n.Inspect(err);
  }

  if (type == v8()->types()->kJSArrayType) {
    JSArray arr(this);
    return pre + arr.Inspect(options, err);
  }

  if (type == v8()->types()->kOddballType) {
    Oddball o(this);
    return pre + o.Inspect(err);
  }

  if (type == v8()->types()->kJSFunctionType) {
    JSFunction fn(this);
    return pre + fn.Inspect(options, err);
  }

  if (type == v8()->types()->kJSRegExpType) {
    JSRegExp re(this);
    return pre + re.Inspect(options, err);
  }

  if (type < v8()->types()->kFirstNonstringType) {
    String str(this);
    return pre + str.Inspect(options, err);
  }

  if (type >= v8()->types()->kFirstContextType &&
      type <= v8()->types()->kLastContextType) {
    Context ctx(this);
    return pre + ctx.Inspect(options, err);
  }

  if (type == v8()->types()->kFixedArrayType) {
    FixedArray arr(this);
    return pre + arr.Inspect(options, err);
  }

  if (type == v8()->types()->kJSArrayBufferType) {
    JSArrayBuffer buf(this);
    return pre + buf.Inspect(options, err);
  }

  if (type == v8()->types()->kJSTypedArrayType) {
    JSArrayBufferView view(this);
    return pre + view.Inspect(options, err);
  }

  if (type == v8()->types()->kJSDateType) {
    JSDate date(this);
    return pre + date.Inspect(err);
  }

  Error::PrintInDebugMode(
      "Unknown HeapObject Type %" PRId64 " at 0x%016" PRIx64 "", type, raw());
  return pre + "<unknown>";
}

inspect_t* HeapObject::InspectX(InspectOptions* options, Error& err) {
  int64_t type = GetType(err);
  if (err.Fail()) return nullptr;

  // TODO(indutny): make this configurable
  char map_address[64];
  char address[64];
  inspect_t* inspect = new inspect_t;
  if (options->print_map) {
    HeapObject map = GetMap(err);
    if (err.Fail()) {
      delete inspect;
      return nullptr;
    }
    snprintf(map_address, sizeof(map_address), "0x%016" PRIx64, map.raw());
    inspect->map_address = map_address;
  }
  snprintf(address, sizeof(address), "0x%016" PRIx64, raw());
  inspect->address = address;

  if (type == v8()->types()->kGlobalObjectType) {
    inspect->type = kGlobalObject;
    inspect->name = "Global";
    return inspect;
  }
  if (type == v8()->types()->kGlobalProxyType) {
    inspect->type = kGlobalProxy;
    inspect->name = "Global proxy";
    return inspect;
  }
  if (type == v8()->types()->kCodeType) {
    inspect->type = kCode;
    inspect->name = "Code";
    return inspect;
  }
  if (type == v8()->types()->kMapType) {
    Map m(this);
    map_t* map = m.InspectX(options, err);
    map->map_address = inspect->map_address;
    map->address = inspect->address;
    delete inspect;
    return map;
  }
  if (type >= v8()->types()->kFirstContextType &&
      type <= v8()->types()->kLastContextType) {
    Context ctx(this);
    context_t* context = ctx.InspectX(options, err);
    if(context == nullptr) {
      delete inspect;
      return nullptr;
    }
    context->map_address = inspect->map_address;
    context->address = inspect->address;
    delete inspect;
    return context;
  }

  if (IsJSErrorType(err)) {
    JSError e(this);
    js_error_t* js_error = e.InspectX(options, err);
    if (js_error == nullptr) {
      delete inspect;
      return nullptr;
    }
    js_error->map_address = inspect->map_address;
    js_error->address = inspect->address;
    delete inspect;
    return js_error;
  }

  if (JSObject::IsObjectType(v8(), type)) {
    JSObject o(this);
    js_object_t* js_object = o.InspectX(options, err);
    if (js_object == nullptr) {
      delete inspect;
      return nullptr;
    }
    js_object->map_address = inspect->map_address;
    js_object->address = inspect->address;
    delete inspect;
    return js_object;
  }

  if (type == v8()->types()->kHeapNumberType) {
    HeapNumber n(this);
    heap_number_t* heap_number = n.InspectX(err);
    if (heap_number == nullptr) {
      delete inspect;
      return nullptr;
    }
    heap_number->map_address = inspect->map_address;
    heap_number->address = inspect->address;
    delete inspect;
    return heap_number;
  }

  if (type == v8()->types()->kJSArrayType) {
    JSArray arr(this);
    js_array_t* js_array = arr.InspectX(options, err);
    if (js_array == nullptr) {
      delete inspect;
      return nullptr;
    }
    js_array->map_address = inspect->map_address;
    js_array->address = inspect->address;
    delete inspect;
    return js_array;
  }

  if (type == v8()->types()->kOddballType) {
    Oddball o(this);
    oddball_t* oddball = o.InspectX(err);
    if (oddball == nullptr) {
      delete inspect;
      return nullptr;
    }
    oddball->map_address = inspect->map_address;
    oddball->address = inspect->address;
    delete inspect;
    return oddball;
  }

  if (type == v8()->types()->kJSFunctionType) {
    JSFunction fn(this);
    js_function_t* js_function = fn.InspectX(options, err);
    if (js_function == nullptr) {
      delete inspect;
      return nullptr;
    }
    js_function->map_address = inspect->map_address;
    js_function->address = inspect->address;
    delete inspect;
    return js_function;
  }

  if (type == v8()->types()->kJSRegExpType) {
    JSRegExp re(this);
    inspect_t* js_regexp = re.InspectX(options, err);
    if (js_regexp == nullptr) {
      delete inspect;
      return nullptr;
    }
    js_regexp->map_address = inspect->map_address;
    js_regexp->address = inspect->address;
    delete inspect;
    return js_regexp;
  }

  if (type < v8()->types()->kFirstNonstringType) {
    String str(this);
    first_non_string_t* string = str.InspectX(options, err);
    if (string == nullptr) {
      delete inspect;
      return nullptr;
    }
    string->map_address = inspect->map_address;
    string->address = inspect->address;
    delete inspect;
    return string;
  }

  if (type >= v8()->types()->kFirstContextType &&
      type <= v8()->types()->kLastContextType) {
    Context ctx(this);
    context_t* context = ctx.InspectX(options, err);
    if (context == nullptr) {
      delete context;
      return nullptr;
    }
    context->map_address = inspect->map_address;
    context->address = inspect->address;
    delete inspect;
    return context;
  }

  if (type == v8()->types()->kFixedArrayType) {
    FixedArray arr(this);
    fixed_array_t* fixed_array = arr.InspectX(options, err);
    if (fixed_array == nullptr) {
      delete inspect;
      return nullptr;
    }
    fixed_array->map_address = inspect->map_address;
    fixed_array->address = inspect->address;
    delete inspect;
    return fixed_array;
  }

  if (type == v8()->types()->kJSArrayBufferType) {
    JSArrayBuffer buf(this);
    js_array_buffer_t* array_buffer = buf.InspectX(options, err);
    if (array_buffer == nullptr) {
      delete inspect;
      return nullptr;
    }
    array_buffer->map_address = inspect->map_address;
    array_buffer->address = inspect->address;
    delete inspect;
    return array_buffer;
  }

  if (type == v8()->types()->kJSTypedArrayType) {
    JSArrayBufferView view(this);
    js_array_buffer_view_t* array_buffer_view = view.InspectX(options, err);
    if (array_buffer_view == nullptr) {
      delete inspect;
      return nullptr;
    }
    array_buffer_view->map_address = inspect->map_address;
    array_buffer_view->address = inspect->address;
    delete inspect;
    return array_buffer_view;
  }

  if (type == v8()->types()->kJSDateType) {
    JSDate date(this);
    js_date_t* js_date = date.InspectX(err);
    if (js_date == nullptr) {
      delete inspect;
      return nullptr;
    }
    js_date->map_address = inspect->map_address;
    js_date->address = inspect->address;
    delete inspect;
    return js_date;
  }

  Error::PrintInDebugMode(
      "Unknown HeapObject Type %" PRId64 " at 0x%016" PRIx64 "", type, raw());
  inspect->type = kUnknown;
  inspect->name = "Unknown";
  return inspect;
}


std::string Smi::ToString(Error& err) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%d", static_cast<int>(GetValue()));
  err = Error::Ok();
  return buf;
}


/* Utility function to generate short type names for objects.
 */
std::string HeapObject::GetTypeName(Error& err) {
  int64_t type = GetType(err);
  if (type == v8()->types()->kGlobalObjectType) return "(Global)";
  if (type == v8()->types()->kGlobalProxyType) return "(Global proxy)";
  if (type == v8()->types()->kCodeType) return "(Code)";
  if (type == v8()->types()->kMapType) {
    return "(Map)";
  }

  if (JSObject::IsObjectType(v8(), type)) {
    v8::HeapObject map_obj = GetMap(err);
    if (err.Fail()) {
      return std::string();
    }

    v8::Map map(map_obj);
    v8::HeapObject constructor_obj = map.Constructor(err);
    if (err.Fail()) {
      return std::string();
    }

    int64_t constructor_type = constructor_obj.GetType(err);
    if (err.Fail()) {
      return std::string();
    }

    if (constructor_type != v8()->types()->kJSFunctionType) {
      return "(Object)";
    }

    v8::JSFunction constructor(constructor_obj);

    return constructor.Name(err);
  }

  if (type == v8()->types()->kHeapNumberType) {
    return "(HeapNumber)";
  }

  if (type == v8()->types()->kJSArrayType) {
    return "(Array)";
  }

  if (type == v8()->types()->kOddballType) {
    return "(Oddball)";
  }

  if (type == v8()->types()->kJSFunctionType) {
    return "(Function)";
  }

  if (type == v8()->types()->kJSRegExpType) {
    return "(RegExp)";
  }

  if (type < v8()->types()->kFirstNonstringType) {
    return "(String)";
  }

  if (type == v8()->types()->kFixedArrayType) {
    return "(FixedArray)";
  }

  if (type == v8()->types()->kJSArrayBufferType) {
    return "(ArrayBuffer)";
  }

  if (type == v8()->types()->kJSTypedArrayType) {
    return "(ArrayBufferView)";
  }

  if (type == v8()->types()->kJSDateType) {
    return "(Date)";
  }

  std::string unknown("unknown: ");

  return unknown + std::to_string(type);
}


std::string Smi::Inspect(Error& err) { return "<Smi: " + ToString(err) + ">"; }

smi_t* Smi::InspectX(Error& err) {
  smi_t* smi = new smi_t;
  smi->type = kSmi;
  smi->name = "Smi";
  smi->value = ToString(err);
  char tmp[128];
  snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, raw());
  smi->address = tmp;
  return smi;
}


std::string HeapNumber::ToString(bool whole, Error& err) {
  char buf[128];
  const char* fmt = whole ? "%f" : "%.2f";
  snprintf(buf, sizeof(buf), fmt, GetValue(err));
  err = Error::Ok();
  return buf;
}


std::string HeapNumber::Inspect(Error& err) {
  return "<Number: " + ToString(true, err) + ">";
}

heap_number_t* HeapNumber::InspectX(Error& err) {
  heap_number_t* heap_number = new heap_number_t;
  heap_number->type = kHeapNumber;
  heap_number->name = "Number";
  heap_number->value = ToString(true, err);
  return heap_number;
}

std::string Symbol::ToString(Error& err) {
  if (!String::IsString(v8(), Name(err), err)) {
    return "Symbol()";
  }
  HeapObject name = Name(err);
  return "Symbol('" + String(name).ToString(err) + "')";
}

std::string String::ToString(Error& err, bool utf16) {
  int64_t repr = Representation(err);
  if (err.Fail()) return std::string();

  int64_t encoding = Encoding(err);
  if (err.Fail()) return std::string();

  if (repr == v8()->string()->kSeqStringTag) {
    if (encoding == v8()->string()->kOneByteStringTag) {
      OneByteString one(this);
      return one.ToString(err);
    } else if (encoding == v8()->string()->kTwoByteStringTag) {
      TwoByteString two(this);
      return two.ToString(err, utf16);
    }

    err = Error::Failure("Unsupported seq string encoding %" PRId64, encoding);
    return std::string();
  }

  if (repr == v8()->string()->kConsStringTag) {
    ConsString cons(this);
    return cons.ToString(err, utf16);
    ;
  }

  if (repr == v8()->string()->kSlicedStringTag) {
    SlicedString sliced(this);
    return sliced.ToString(err, utf16);
  }

  // TODO(indutny): add support for external strings
  if (repr == v8()->string()->kExternalStringTag) {
    return std::string("(external)");
  }

  if (repr == v8()->string()->kThinStringTag) {
    ThinString thin(this);
    return thin.ToString(err, utf16);
  }

  err = Error::Failure("Unsupported string representation %" PRId64, repr);
  return std::string();
}


std::string String::Inspect(InspectOptions* options, Error& err) {
  std::string val = ToString(err);
  if (err.Fail()) return std::string();

  unsigned int len = options->length;
  int total_length = val.length();
  if (len != 0 && val.length() > len) val = val.substr(0, len) + "...";

  return "<String \"" + val + "\", length=" + std::to_string(total_length) +
         ">";
}

unsigned long String::GetSubStr(unsigned long current, int limit,
                                std::string val) {
  unsigned long curt = current + limit;
  int next = val[curt] - 0xFFFFFF00;
  if (val.length() > curt + 1 && next < 192 && next > 127) {
    return GetSubStr(current, limit + 1, val);
  } else {
    return limit;
  }
}

first_non_string_t* String::InspectX(InspectOptions* options, Error& err) {
  std::string val = ToString(err, false);
  if (err.Fail()) return nullptr;
  first_non_string_t* string = new first_non_string_t;
  string->type = kFirstNonstring;
  string->name = "String";
  string->total_length = val.length();

  unsigned int len = options->length;
  if (options->current > val.length() - 1) {
    string->end = true;
    string->current = val.length();
    val = "";
  } else if (options->current != 0 && options->limit != 0) {
    int option_current = options->current;
    if (option_current < 0) option_current = 0;
    int option_limit = options->limit;
    if (option_limit < 0) option_limit = 0;
    unsigned int next_current = option_current + option_limit;
    if (val.length() > next_current) {
      unsigned long limit = GetSubStr(option_current, option_limit, val);
      val = val.substr(option_current, limit) + "...";
      string->current = option_current + limit;
      string->end = false;
    } else {
      string->current = val.length();
      val = val.substr(options->current);
      string->end = true;
    }
  } else {
    if (len != 0 && val.length() > len) {
      val = val.substr(0, len) + "...";
      string->end = false;
      string->current = len;
    } else {
      string->current = val.length();
      string->end = true;
    }
  }

  string->display_value = val;
  return string;
}


std::string FixedArray::Inspect(InspectOptions* options, Error& err) {
  Smi length_smi = Length(err);
  if (err.Fail()) return std::string();

  std::string res = "<FixedArray, len=" + length_smi.ToString(err);
  if (err.Fail()) return std::string();

  if (options->detailed) {
    std::string contents = InspectContents(length_smi.GetValue(), err);
    if (!contents.empty()) res += " contents={\n" + contents + "}";
  }

  return res + ">";
}

fixed_array_t* FixedArray::InspectX(InspectOptions* options, Error& err) {
  Smi length_smi = Length(err);
  length_smi.ToString(err);
  if (err.Fail()) return nullptr;
  fixed_array_t* fixed_array = new fixed_array_t;
  fixed_array->type = kFixedArray;
  fixed_array->name = "FixedArray";
  int64_t total_length = length_smi.GetValue();
  fixed_array->total_length = total_length;
  if (options->detailed) {
    int option_current = options->current;
    if (option_current < 0) option_current = 0;
    int option_limit = options->limit;
    if (option_limit < 0) option_limit = 0;
    int64_t start = option_current;
    if (start >= total_length) start = total_length;
    int64_t end = total_length;
    if (option_limit != 0) end = option_current + option_limit;
    if (end >= total_length) end = total_length;
    fixed_array->current = end;
    fixed_array->length = end - start;
    fixed_array->elements = new inspect_t*[fixed_array->length];
    InspectOptions opt;
    for (int64_t i = start; i < end; ++i) {
      Value value = Get<Value>(i, err);
      if (err.Fail()) {
        delete fixed_array;
        return nullptr;
      }
      fixed_array->elements[i - start] = value.InspectX(&opt, err);
      if (err.Fail()) {
        delete fixed_array;
        return nullptr;
      }
    }
  } else {
    fixed_array->elements = nullptr;
  }

  return fixed_array;
}

std::string FixedArray::InspectContents(int length, Error& err) {
  std::string res;
  InspectOptions options;

  for (int i = 0; i < length; i++) {
    Value value = Get<Value>(i, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    char tmp[128];
    snprintf(tmp, sizeof(tmp), "    [%d]=", i);
    res += tmp + value.Inspect(&options, err);
    if (err.Fail()) return std::string();
  }

  return res;
}

// Context locals iterator implementations
Context::Locals::Locals(Context* context, Error& err) {
  context_ = context;
  HeapObject scope_obj = context_->GetScopeInfo(err);
  if (err.Fail()) return;

  scope_info_ = ScopeInfo(scope_obj);
  Smi param_count_smi = scope_info_.ParameterCount(err);
  if (err.Fail()) return;
  Smi stack_count_smi = scope_info_.StackLocalCount(err);
  if (err.Fail()) return;
  Smi local_count_smi = scope_info_.ContextLocalCount(err);
  if (err.Fail()) return;

  param_count_ = param_count_smi.GetValue();
  stack_count_ = stack_count_smi.GetValue();
  local_count_ = local_count_smi.GetValue();
}

Context::Locals::Iterator Context::Locals::begin() { return Iterator(0, this); }

Context::Locals::Iterator Context::Locals::end() {
  return Iterator(local_count_, this);
}

const Context::Locals::Iterator Context::Locals::Iterator::operator++(int) {
  current_++;
  return Iterator(current_, this->outer_);
}

bool Context::Locals::Iterator::operator!=(Context::Locals::Iterator that) {
  return current_ != that.current_ || outer_->context_ != that.outer_->context_;
}

v8::Value Context::Locals::Iterator::operator*() {
  Error err;
  return outer_->context_->ContextSlot(current_, err);
}

String Context::Locals::Iterator::LocalName(Error& err) {
  return outer_->scope_info_.ContextLocalName(current_, outer_->param_count_,
                                              outer_->stack_count_, err);
}

Value Context::Locals::Iterator::GetValue(Error& err) {
  return outer_->context_->ContextSlot(current_, err);
}

std::string Context::Inspect(InspectOptions* options, Error& err) {
  // Not enough postmortem information, return bare minimum
  if (v8()->shared_info()->kScopeInfoOffset == -1 &&
      v8()->shared_info()->kNameOrScopeInfoOffset == -1)
    return std::string();

  std::string res = "<Context";

  if (!options->detailed) {
    return res + ">";
  }

  res += ": {\n";

  Value previous = Previous(err);
  if (err.Fail()) return std::string();

  HeapObject scope_obj = GetScopeInfo(err);
  if (err.Fail()) return std::string();

  ScopeInfo scope(scope_obj);

  HeapObject heap_previous = HeapObject(previous);
  if (heap_previous.Check()) {
    char tmp[128];
    snprintf(
        tmp, sizeof(tmp),
        (options->get_indent_spaces() + "(previous)=0x%016" PRIx64).c_str(),
        previous.raw());
    res += std::string(tmp) + ":<Context>,";
  }

  if (!res.empty()) res += "\n";

  if (v8()->context()->hasClosure()) {
    JSFunction closure = Closure(err);
    if (err.Fail()) return std::string();
    char tmp[128];
    snprintf(
        tmp, sizeof(tmp),
        (options->get_indent_spaces() + "(closure)=0x%016" PRIx64 " {").c_str(),
        closure.raw());
    res += tmp;

    InspectOptions closure_options;
    res += closure.Inspect(&closure_options, err) + "}";
    if (err.Fail()) return std::string();
  } else {
    char tmp[128];
    snprintf(
        tmp, sizeof(tmp),
        (options->get_indent_spaces() + "(scope_info)=0x%016" PRIx64).c_str(),
        scope.raw());

    res += std::string(tmp) + ":<ScopeInfo";

    Error function_name_error;
    HeapObject maybe_function_name =
        scope.MaybeFunctionName(function_name_error);

    if (function_name_error.Success()) {
      res += ": for function " + String(maybe_function_name).ToString(err);
    }

    res += ">";
  }

  Context::Locals locals(this, err);
  if (err.Fail()) return std::string();
  for (v8::Context::Locals::Iterator it = locals.begin(); it != locals.end();
       it++) {
    String name = it.LocalName(err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    res += options->get_indent_spaces() + name.ToString(err) + "=";
    if (err.Fail()) return std::string();

    Value value = it.GetValue(err);
    if (err.Fail()) return std::string();

    InspectOptions val_options;
    res += value.Inspect(&val_options, err);
    if (err.Fail()) return std::string();
  }

  return res + "}>";
}

context_t* Context::InspectX(InspectOptions* options, Error& err) {
  // Not enough postmortem information, return bare minimum
  if (v8()->shared_info()->kScopeInfoOffset == -1 &&
      v8()->shared_info()->kNameOrScopeInfoOffset == -1)
    return nullptr;

  context_t* context = new context_t;
  context->type = kContext;
  context->name = "Context";

  if (!options->detailed) {
    return context;
  }

  Value previous = Previous(err);
  if (err.Fail()) return nullptr;

  HeapObject scope_obj = GetScopeInfo(err);
  if (err.Fail()) return nullptr;

  ScopeInfo scope(scope_obj);

  Smi param_count_smi = scope.ParameterCount(err);
  if (err.Fail()) return nullptr;
  Smi stack_count_smi = scope.StackLocalCount(err);
  if (err.Fail()) return nullptr;
  Smi local_count_smi = scope.ContextLocalCount(err);
  if (err.Fail()) return nullptr;

  HeapObject heap_previous = HeapObject(previous);
  if (heap_previous.Check()) {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, previous.raw());
    context->previous_address = tmp;
  }

  if (v8()->context()->hasClosure()) {
    JSFunction closure = Closure(err);
    if (err.Fail()) return nullptr;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, closure.raw());
    context->closure_address = tmp;

    InspectOptions closure_options;
    context->closure = closure.InspectX(&closure_options, err);
    if (err.Fail()) {
      delete context;
      return nullptr;
    }
  } else {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, scope.raw());
    context->scope_info_address = tmp;

    Error function_name_error;
    HeapObject maybe_function_name =
        scope.MaybeFunctionName(function_name_error);
    InspectOptions may_be_function_options;
    context->may_be_function = maybe_function_name.InspectX(
        &may_be_function_options, function_name_error);
    if (function_name_error.Fail()) {
      delete context;
      return nullptr;
    }
  }

  int param_count = param_count_smi.GetValue();
  int stack_count = stack_count_smi.GetValue();
  int local_count = local_count_smi.GetValue();
  properties_t* scope_object = new properties_t;
  property_t** object_list = new property_t*[local_count];
  scope_object->length = local_count;
  scope_object->properties = object_list;
  context->scope_object = scope_object;
  for (int i = 0; i < local_count; i++) {
    String name = scope.ContextLocalName(i, param_count, stack_count, err);
    if (err.Fail()) {
      delete context;
      return nullptr;
    }

    property_t* object = new property_t;
    object_list[i] = object;
    object->key = name.ToString(err);
    if (err.Fail()) {
      delete context;
      return nullptr;
    }

    Value value = ContextSlot(i, err);
    if (err.Fail()) {
      delete context;
      return nullptr;
    }

    InspectOptions val_options;
    object->value = value.InspectX(&val_options, err);
    if (err.Fail()) {
      delete context;
      return nullptr;
    }
  }
  return context;
}


std::string Oddball::Inspect(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return std::string();

  int64_t kind_val = kind.GetValue();
  if (kind_val == v8()->oddball()->kException) return "<exception>";
  if (kind_val == v8()->oddball()->kFalse) return "false";
  if (kind_val == v8()->oddball()->kTrue) return "true";
  if (kind_val == v8()->oddball()->kUndefined) return "undefined";
  if (kind_val == v8()->oddball()->kNull) return "null";
  if (kind_val == v8()->oddball()->kTheHole) return "<hole>";
  if (kind_val == v8()->oddball()->kUninitialized) return "<uninitialized>";
  return "<Oddball>";
}

oddball_t* Oddball::InspectX(Error& err) {
  Smi kind = Kind(err);
  if (err.Fail()) return nullptr;

  oddball_t* oddball = new oddball_t;
  oddball->type = kOddball;
  oddball->name = "Oddball";
  oddball->value = "";

  int64_t kind_val = kind.GetValue();
  if (kind_val == v8()->oddball()->kException) oddball->value = "<exception>";
  if (kind_val == v8()->oddball()->kFalse) oddball->value = "false";
  if (kind_val == v8()->oddball()->kTrue) oddball->value = "true";
  if (kind_val == v8()->oddball()->kUndefined) oddball->value = "undefined";
  if (kind_val == v8()->oddball()->kNull) oddball->value = "null";
  if (kind_val == v8()->oddball()->kTheHole) oddball->value = "<hole>";
  if (kind_val == v8()->oddball()->kUninitialized)
    oddball->value = "<uninitialized>";

  return oddball;
}

std::string JSArrayBuffer::Inspect(InspectOptions* options, Error& err) {
  bool neutered = WasNeutered(err);
  if (err.Fail()) return std::string();

  if (neutered) return "<ArrayBuffer [neutered]>";

  int64_t data = BackingStore(err);
  if (err.Fail()) return std::string();

  Smi length = ByteLength(err);
  if (err.Fail()) return std::string();

  int byte_length = static_cast<int>(length.GetValue());

  char tmp[128];
  snprintf(tmp, sizeof(tmp),
           "<ArrayBuffer: backingStore=0x%016" PRIx64 ", byteLength=%d", data,
           byte_length);

  std::string res;
  res += tmp;
  if (options->detailed) {
    res += ": [\n  ";

    int display_length = std::min<int>(byte_length, options->length);
    res += v8()->LoadBytes(data, display_length, err);

    if (display_length < byte_length) {
      res += " ...";
    }
    res += "\n]>";
  } else {
    res += ">";
  }

  return res;
}

js_array_buffer_t* JSArrayBuffer::InspectX(InspectOptions* options,
                                           Error& err) {
  bool neutered = WasNeutered(err);
  if (err.Fail()) return nullptr;

  js_array_buffer_t* array_buffer = new js_array_buffer_t;
  array_buffer->type = kJsArrayBuffer;
  array_buffer->name = "ArrayBuffer";

  if (neutered) {
    array_buffer->neutered = true;
    return array_buffer;
  }
  array_buffer->neutered = false;
  int64_t data = BackingStore(err);
  if (err.Fail()) {
    delete array_buffer;
    return nullptr;
  }

  Smi length = ByteLength(err);
  if (err.Fail()) {
    delete array_buffer;
    return nullptr;
  }

  int byte_length = static_cast<int>(length.GetValue());
  array_buffer->byte_length = byte_length;

  char tmp[128];
  snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, data);
  array_buffer->backing_store_address = tmp;

  if (options->detailed) {
    int option_current = options->current;
    if (option_current < 0) option_current = 0;
    int option_limit = options->limit;
    if (option_limit < 0) option_limit = 0;
    int64_t start = option_current;
    if (start >= byte_length) start = byte_length;
    int64_t end = byte_length;
    if (option_limit != 0) end = option_current + option_limit;
    if (end >= byte_length) end = byte_length;
    array_buffer->current = end;
    array_buffer->display_length = end - start;
    array_buffer->elements =
        v8()->LoadBytesX(data, byte_length, start, end, err);
  } else {
    array_buffer->elements = nullptr;
  }

  return array_buffer;
}


std::string JSArrayBufferView::Inspect(InspectOptions* options, Error& err) {
  JSArrayBuffer buf = Buffer(err);
  if (err.Fail()) return std::string();

  bool neutered = buf.WasNeutered(err);
  if (err.Fail()) return std::string();

  if (neutered) return "<ArrayBufferView [neutered]>";

  int64_t data = buf.BackingStore(err);
  if (err.Fail()) return std::string();

  if (data == 0) {
    // The backing store has not been materialized yet.
    HeapObject elements_obj = Elements(err);
    if (err.Fail()) return std::string();
    FixedTypedArrayBase elements(elements_obj);
    int64_t base = elements.GetBase(err);
    if (err.Fail()) return std::string();
    int64_t external = elements.GetExternal(err);
    if (err.Fail()) return std::string();
    data = base + external;
  }

  Smi off = ByteOffset(err);
  if (err.Fail()) return std::string();

  Smi length = ByteLength(err);
  if (err.Fail()) return std::string();

  int byte_length = static_cast<int>(length.GetValue());
  int byte_offset = static_cast<int>(off.GetValue());
  char tmp[128];
  snprintf(tmp, sizeof(tmp),
           "<ArrayBufferView: backingStore=0x%016" PRIx64
           ", byteOffset=%d, byteLength=%d",
           data, byte_offset, byte_length);

  std::string res;
  res += tmp;
  if (options->detailed) {
    res += ": [\n  ";

    int display_length = std::min<int>(byte_length, options->length);
    res += v8()->LoadBytes(data + byte_offset, display_length, err);

    if (display_length < byte_length) {
      res += " ...";
    }
    res += "\n]>";
  } else {
    res += ">";
  }
  return res;
}

js_array_buffer_view_t* JSArrayBufferView::InspectX(InspectOptions* options,
                                                    Error& err) {
  JSArrayBuffer buf = Buffer(err);
  if (err.Fail()) return nullptr;

  bool neutered = buf.WasNeutered(err);
  if (err.Fail()) return nullptr;

  js_array_buffer_view_t* array_buffer_view = new js_array_buffer_view_t;
  array_buffer_view->type = kJsArrayBufferView;
  array_buffer_view->name = "ArrayBufferView";

  if (neutered) {
    array_buffer_view->neutered = true;
    return array_buffer_view;
  }
  array_buffer_view->neutered = false;

  int64_t data = buf.BackingStore(err);
  if (err.Fail()) {
    delete array_buffer_view;
    return nullptr;
  }

  if (data == 0) {
    // The backing store has not been materialized yet.
    HeapObject elements_obj = Elements(err);
    if (err.Fail()) {
      delete array_buffer_view;
      return nullptr;
    }
    FixedTypedArrayBase elements(elements_obj);
    int64_t base = elements.GetBase(err);
    if (err.Fail()) {
      delete array_buffer_view;
      return nullptr;
    }
    int64_t external = elements.GetExternal(err);
    if (err.Fail()) {
      delete array_buffer_view;
      return nullptr;
    }
    data = base + external;
  }

  Smi off = ByteOffset(err);
  if (err.Fail()) {
    delete array_buffer_view;
    return nullptr;
  }

  Smi length = ByteLength(err);
  if (err.Fail()) {
    delete array_buffer_view;
    return nullptr;
  }

  int byte_length = static_cast<int>(length.GetValue());
  array_buffer_view->byte_length = byte_length;
  int byte_offset = static_cast<int>(off.GetValue());
  array_buffer_view->byte_offset = byte_offset;
  char tmp[128];
  snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, data);
  array_buffer_view->backing_store_address = tmp;

  if (options->detailed) {
    int option_current = options->current;
    if (option_current < 0) option_current = 0;
    int option_limit = options->limit;
    if (option_limit < 0) option_limit = 0;
    int64_t start = option_current;
    if (start >= byte_length) start = byte_length;
    int64_t end = byte_length;
    if (option_limit != 0) end = option_current + option_limit;
    if (end >= byte_length) end = byte_length;
    array_buffer_view->current = end;
    array_buffer_view->display_length = end - start;
    array_buffer_view->elements =
        v8()->LoadBytesX(data + byte_offset, byte_length, start, end, err);
  } else {
    array_buffer_view->elements = nullptr;
  }

  return array_buffer_view;
}


std::string Map::Inspect(InspectOptions* options, Error& err) {
  HeapObject descriptors_obj = InstanceDescriptors(err);
  if (err.Fail()) return std::string();

  int64_t own_descriptors_count = NumberOfOwnDescriptors(err);
  if (err.Fail()) return std::string();

  std::string in_object_properties_or_constructor;
  int64_t in_object_properties_or_constructor_index;
  if (IsJSObjectMap(err)) {
    if (err.Fail()) return std::string();
    in_object_properties_or_constructor_index = InObjectProperties(err);
    in_object_properties_or_constructor = std::string("in_object_size");
  } else {
    in_object_properties_or_constructor_index = ConstructorFunctionIndex(err);
    in_object_properties_or_constructor = std::string("constructor_index");
  }
  if (err.Fail()) return std::string();

  int64_t instance_size = InstanceSize(err);
  if (err.Fail()) return std::string();

  char tmp[256];
  snprintf(tmp, sizeof(tmp),
           "<Map own_descriptors=%d %s=%d instance_size=%d "
           "descriptors=0x%016" PRIx64,
           static_cast<int>(own_descriptors_count),
           in_object_properties_or_constructor.c_str(),
           static_cast<int>(in_object_properties_or_constructor_index),
           static_cast<int>(instance_size), descriptors_obj.raw());
  if (!options->detailed) {
    return std::string(tmp) + ">";
  }

  DescriptorArray descriptors(descriptors_obj);
  if (err.Fail()) return std::string();

  return std::string(tmp) + ":" + descriptors.Inspect(options, err) + ">";
}

map_t* Map::InspectX(InspectOptions* options, Error& err) {
  HeapObject descriptors_obj = InstanceDescriptors(err);
  if (err.Fail()) return nullptr;

  int64_t own_descriptors_count = NumberOfOwnDescriptors(err);
  if (err.Fail()) return nullptr;

  std::string in_object_properties_or_constructor;
  int64_t in_object_properties_or_constructor_index;
  if (IsJSObjectMap(err)) {
    if (err.Fail()) return nullptr;
    in_object_properties_or_constructor_index = InObjectProperties(err);
    in_object_properties_or_constructor = std::string("in_object_size");
  } else {
    in_object_properties_or_constructor_index = ConstructorFunctionIndex(err);
    in_object_properties_or_constructor = std::string("constructor_index");
  }
  if (err.Fail()) return nullptr;

  int64_t instance_size = InstanceSize(err);
  if (err.Fail()) return nullptr;

  map_t* map = new map_t;
  map->type = kMap;
  map->name = "Map";
  map->own_descriptors = static_cast<int>(own_descriptors_count);
  map->in_object_properties_or_constructor =
      in_object_properties_or_constructor;
  map->in_object_properties_or_constructor_index =
      static_cast<int>(in_object_properties_or_constructor_index);
  map->instance_size = static_cast<int>(instance_size);
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, descriptors_obj.raw());
  map->descriptors_address = tmp;
  if (options->detailed) {
    // Add DescriptorArray
    DescriptorArray descriptors(descriptors_obj);
    map->descriptors_array = descriptors.InspectX(options, err);
  } else {
    map->descriptors_array = nullptr;
  }
  return map;
}

HeapObject Map::Constructor(Error& err) {
  Map current = this;

  do {
    HeapObject obj = current.MaybeConstructor(err);
    if (err.Fail()) return current;

    int64_t type = obj.GetType(err);
    if (err.Fail()) return current;

    current = obj;
    if (type != v8()->types()->kMapType) break;
  } while (true);

  return current;
}


std::string JSObject::Inspect(InspectOptions* options, Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return std::string();

  Map map(map_obj);
  HeapObject constructor_obj = map.Constructor(err);
  if (err.Fail()) return std::string();

  int64_t constructor_type = constructor_obj.GetType(err);
  if (err.Fail()) return std::string();

  if (constructor_type != v8()->types()->kJSFunctionType)
    return "<Object: no constructor>";

  JSFunction constructor(constructor_obj);

  std::string res = "<Object: " + constructor.Name(err);
  if (err.Fail()) return std::string();

  res += InspectAllProperties(options, err);
  res += ">";
  return res;
}

std::string JSObject::InspectAllProperties(InspectOptions* options,
                                           Error& err) {
  std::string res = std::string();
  // Print properties in detailed mode
  if (options->detailed) {
    res += " " + InspectProperties(err);
    if (err.Fail()) return std::string();

    std::string fields = InspectInternalFields(err);
    if (err.Fail()) return std::string();

    if (!fields.empty()) {
      res += "\n  internal fields {" + fields + "}";
    }
    return res;
  }

  return res;
}

js_error_t* JSError::InspectX(InspectOptions* options, Error& err) {
  js_object_t* js_object = JSObject::InspectX(options, err);
  js_error_t* js_error = new js_error_t;
  js_error->type = kJsError;
  js_error->name = "Error";
  js_error->constructor = js_object->constructor;
  js_error->elements_length = js_object->elements_length;
  js_error->properties_length = js_object->properties_length;
  js_error->fields_length = js_object->fields_length;
  js_error->elements = js_object->elements;
  js_error->properties = js_object->properties;
  js_error->fields = js_object->fields;

  if (options->detailed) {
    InspectOptions simple;

    // TODO (mmarchini): once we have Symbol support we'll need to search for
    // <unnamed symbol>, since the stack symbol doesn't have an external name.
    // In the future we can add postmortem metadata on V8 regarding existing
    // symbols, but for now we'll use an heuristic to find the stack in the
    // error object.
    Value maybe_stack = GetProperty("<non-string>", err);

    if (err.Fail()) {
      Error::PrintInDebugMode(
          "Couldn't find a symbol property in the Error object.");
      return js_error;
    }

    int64_t type = HeapObject(maybe_stack).GetType(err);

    if (err.Fail()) {
      Error::PrintInDebugMode("Symbol property references an invalid object.");
      return js_error;
    }

    // NOTE (mmarchini): The stack is stored as a JSArray
    if (type != v8()->types()->kJSArrayType) {
      Error::PrintInDebugMode("Symbol property doesn't have the right type.");
      return js_error;
    }

    JSArray arr(maybe_stack);

    Value maybe_stack_len = arr.GetArrayElement(0, err);

    if (err.Fail()) {
      Error::PrintInDebugMode(
          "Couldn't get the first element from the stack array");
      return js_error;
    }

    int64_t stack_len = Smi(maybe_stack_len).GetValue();

    int multiplier = 5;
    // On Node.js v8.x, the first array element is the stack size, and each
    // stack frame use 5 elements.
    if ((stack_len * multiplier + 1) != arr.GetArrayLength(err)) {
      // On Node.js v6.x, the first array element is zero, and each stack frame
      // use 4 element.
      multiplier = 4;
      if ((stack_len != 0) ||
          ((arr.GetArrayLength(err) - 1) % multiplier != 0)) {
        Error::PrintInDebugMode(
            "JSArray doesn't look like a Stack Frames array. stack_len: %lld "
            "array_len: %lld",
            stack_len, arr.GetArrayLength(err));
        return js_error;
      }
      stack_len = (arr.GetArrayLength(err) - 1) / multiplier;
    }

    js_error->stack_length = stack_len;
    std::string* stacks = new std::string[stack_len];
    js_error->stacks = stacks;
    // TODO (mmarchini): Refactor: create an StackIterator which returns
    // StackFrame objects
    for (int64_t i = 0; i < stack_len; i++) {
      Value maybe_fn = arr.GetArrayElement(2 + (i * multiplier), err);
      if (err.Fail()) {
        stacks[i] = "<unknown>";
        continue;
      }

      stacks[i] = "    " + HeapObject(maybe_fn).Inspect(&simple, err);
    }
  }

  return js_error;
}

std::string JSError::InspectAllProperties(InspectOptions* options, Error& err) {
  std::string res = JSObject::InspectAllProperties(options, err);
  if (options->detailed) {
    InspectOptions simple;

    // TODO (mmarchini): once we have Symbol support we'll need to search for
    // <unnamed symbol>, since the stack symbol doesn't have an external name.
    // In the future we can add postmortem metadata on V8 regarding existing
    // symbols, but for now we'll use an heuristic to find the stack in the
    // error object.
    Value maybe_stack = GetProperty("<non-string>", err);

    if (err.Fail()) {
      Error::PrintInDebugMode(
          "Couldn't find a symbol property in the Error object.");
      return res;
    }

    int64_t type = HeapObject(maybe_stack).GetType(err);

    if (err.Fail()) {
      Error::PrintInDebugMode("Symbol property references an invalid object.");
      return res;
    }

    // NOTE (mmarchini): The stack is stored as a JSArray
    if (type != v8()->types()->kJSArrayType) {
      Error::PrintInDebugMode("Symbol property doesn't have the right type.");
      return res;
    }

    JSArray arr(maybe_stack);

    Value maybe_stack_len = arr.GetArrayElement(0, err);

    if (err.Fail()) {
      Error::PrintInDebugMode(
          "Couldn't get the first element from the stack array");
      return res;
    }

    int64_t stack_len = Smi(maybe_stack_len).GetValue();

    int multiplier = 5;
    // On Node.js v8.x, the first array element is the stack size, and each
    // stack frame use 5 elements.
    if ((stack_len * multiplier + 1) != arr.GetArrayLength(err)) {
      // On Node.js v6.x, the first array element is zero, and each stack frame
      // use 4 element.
      multiplier = 4;
      if ((stack_len != 0) ||
          ((arr.GetArrayLength(err) - 1) % multiplier != 0)) {
        Error::PrintInDebugMode(
            "JSArray doesn't look like a Stack Frames array. stack_len: %lld "
            "array_len: %lld",
            stack_len, arr.GetArrayLength(err));
        return res;
      }
      stack_len = (arr.GetArrayLength(err) - 1) / multiplier;
    }

    res += "  error stack {";

    // TODO (mmarchini): Refactor: create an StackIterator which returns
    // StackFrame objects
    for (int64_t i = 0; i < stack_len; i++) {
      Value maybe_fn = arr.GetArrayElement(2 + (i * multiplier), err);
      if (err.Fail()) {
        res += "    <unknown>";
        continue;
      }

      res += "    ";
      res += HeapObject(maybe_fn).Inspect(&simple, err);
    }
    res += "  }";
  }
  return res;
}

js_object_t* JSObject::InspectX(InspectOptions* options, Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return nullptr;

  Map map(map_obj);
  HeapObject constructor_obj = map.Constructor(err);
  if (err.Fail()) return nullptr;

  int64_t constructor_type = constructor_obj.GetType(err);
  if (err.Fail()) return nullptr;

  js_object_t* js_object = new js_object_t;
  js_object->type = kJsObject;
  js_object->name = "Object";
  if (constructor_type != v8()->types()->kJSFunctionType) {
    js_object->constructor = "no constructor";
    return js_object;
  }

  JSFunction constructor(constructor_obj);

  js_object->constructor = constructor.Name(err);
  if (err.Fail()) {
    delete js_object;
    return nullptr;
  }

  // set elements, properties and internal fields length
  js_object->elements_length = GetElementsLength(err);
  js_object->properties_length = GetPropertiesLength(err);
  js_object->fields_length = GetFieldsLength(err);

  if (options->detailed) {
    int option_current = options->current;
    if (option_current < 0) option_current = 0;
    int option_limit = options->limit;
    if (option_limit < 0) option_limit = 0;
    int option_end = option_current + option_limit;

    // add elements
    if (option_current < js_object->elements_length) {
      js_object->elements = InspectElementsX(err, option_current, option_limit);
      if (err.Fail()) {
        delete js_object;
        return nullptr;
      }
    } else {
      js_object->elements = nullptr;
    }

    // add properties
    if (option_current >= js_object->elements_length) {
      HeapObject map_obj = GetMap(err);
      if (err.Fail()) {
        delete js_object;
        return nullptr;
      }
      Map map(map_obj);
      bool is_dict = map.IsDictionary(err);
      if (err.Fail()) {
        delete js_object;
        return nullptr;
      }
      if (is_dict)
        js_object->properties = InspectDictionaryX(
            err, option_current - js_object->elements_length, option_limit);
      else
        js_object->properties = InspectDescriptorsX(
            map, err, option_current - js_object->elements_length,
            option_limit);
      if (err.Fail()) {
        delete js_object;
        return nullptr;
      }
    } else if (option_current < js_object->elements_length &&
               option_end >= js_object->elements_length) {
      HeapObject map_obj = GetMap(err);
      if (err.Fail()) {
        delete js_object;
        return nullptr;
      }
      Map map(map_obj);
      bool is_dict = map.IsDictionary(err);
      if (err.Fail()) {
        delete js_object;
        return nullptr;
      }
      if (is_dict)
        js_object->properties =
            InspectDictionaryX(err, 0, option_end - js_object->elements_length);
      else
        js_object->properties = InspectDescriptorsX(
            map, err, 0, option_end - js_object->elements_length);
      if (err.Fail()) {
        delete js_object;
        return nullptr;
      }
    } else {
      js_object->properties = nullptr;
    }

    // add internal fields
    if (option_current >=
            js_object->elements_length + js_object->properties_length &&
        option_current < js_object->elements_length +
                             js_object->properties_length +
                             js_object->fields_length) {
      js_object->fields =
          InspectInternalFieldsX(err,
                                 option_current - js_object->elements_length -
                                     js_object->properties_length,
                                 option_limit);
    } else if (option_current <
                   js_object->elements_length + js_object->properties_length &&
               option_end >=
                   js_object->elements_length + js_object->properties_length) {
      js_object->fields =
          InspectInternalFieldsX(err, 0,
                                 option_end - js_object->elements_length -
                                     js_object->properties_length);
    } else {
      js_object->fields = nullptr;
    }
    if (err.Fail()) {
      delete js_object;
      return nullptr;
    }
  }

  return js_object;
}

std::string JSObject::InspectInternalFields(Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return std::string();

  Map map(map_obj);
  int64_t type = map.GetType(err);
  if (err.Fail()) return std::string();

  // Only JSObject for now
  if (!JSObject::IsObjectType(v8(), type)) return std::string();

  int64_t instance_size = map.InstanceSize(err);

  // kVariableSizeSentinel == 0
  // TODO(indutny): post-mortem constant for this?
  if (err.Fail() || instance_size == 0) return std::string();

  int64_t in_object_props = map.InObjectProperties(err);
  if (err.Fail()) return std::string();

  // in-object properties are appended to the end of the JSObject,
  // skip them.
  instance_size -= in_object_props * v8()->common()->kPointerSize;

  std::string res;
  for (int64_t off = v8()->js_object()->kInternalFieldsOffset;
       off < instance_size; off += v8()->common()->kPointerSize) {
    int64_t field = LoadField(off, err);
    if (err.Fail()) return std::string();

    char tmp[128];
    snprintf(tmp, sizeof(tmp), "    0x%016" PRIx64, field);

    if (!res.empty()) res += ",\n  ";
    res += tmp;
  }

  return res;
}

internal_fileds_t* JSObject::InspectInternalFieldsX(Error& err, int64_t current,
                                                    int64_t limit) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return nullptr;

  Map map(map_obj);
  int64_t type = map.GetType(err);
  if (err.Fail()) return nullptr;

  // Only JSObject for now
  if (!JSObject::IsObjectType(v8(), type)) return nullptr;

  int64_t instance_size = map.InstanceSize(err);

  // kVariableSizeSentinel == 0
  // TODO(indutny): post-mortem constant for this?
  if (err.Fail() || instance_size == 0) return nullptr;

  int64_t in_object_props = map.InObjectProperties(err);
  if (err.Fail()) return nullptr;

  // in-object properties are appended to the end of the JSObject,
  // skip them.
  instance_size -= in_object_props * v8()->common()->kPointerSize;
  // calculate length
  int64_t length = 0;
  for (int64_t off = v8()->js_object()->kInternalFieldsOffset;
       off < instance_size; off += v8()->common()->kPointerSize)
    ++length;

  int64_t start = current;
  if (start >= length) start = length;
  int64_t end = length;
  if (limit != 0) end = current + limit;
  if (end >= length) end = length;

  internal_fileds_t* fields = new internal_fileds_t;
  internal_filed_t** fieldtmp = new internal_filed_t*[end - start];
  fields->length = end - start;
  fields->internal_fileds = fieldtmp;
  fields->current = end;
  int64_t i = 0;
  for (int64_t off = v8()->js_object()->kInternalFieldsOffset;
       off < instance_size; off += v8()->common()->kPointerSize) {
    int64_t field = LoadField(off, err);
    if (err.Fail()) {
      delete fields;
      return nullptr;
    }

    char tmp[128];
    snprintf(tmp, sizeof(tmp), "0x%016" PRIx64, field);
    internal_filed_t* tmp2 = new internal_filed_t;
    tmp2->address = tmp;
    if (i < length && i >= start && i < end) fieldtmp[i - start] = tmp2;
    if (i < length) i++;
  }

  return fields;
}

int64_t JSObject::GetFieldsLength(Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return 0;

  Map map(map_obj);
  int64_t type = map.GetType(err);
  if (err.Fail()) return 0;

  // Only JSObject for now
  if (!JSObject::IsObjectType(v8(), type)) return 0;

  int64_t instance_size = map.InstanceSize(err);

  // kVariableSizeSentinel == 0
  // TODO(indutny): post-mortem constant for this?
  if (err.Fail() || instance_size == 0) return 0;

  int64_t in_object_props = map.InObjectProperties(err);
  if (err.Fail()) return 0;

  // in-object properties are appended to the end of the JSObject,
  // skip them.
  instance_size -= in_object_props * v8()->common()->kPointerSize;
  // calculate length
  int64_t length = 0;
  for (int64_t off = v8()->js_object()->kInternalFieldsOffset;
       off < instance_size; off += v8()->common()->kPointerSize)
    ++length;
  return length;
}

std::string JSObject::InspectProperties(Error& err) {
  std::string res;

  std::string elems = InspectElements(err);
  if (err.Fail()) return std::string();

  if (!elems.empty()) res = "elements {\n" + elems + "}";

  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return std::string();

  Map map(map_obj);

  std::string props;
  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return std::string();

  if (is_dict)
    props = InspectDictionary(err);
  else
    props = InspectDescriptors(map, err);

  if (err.Fail()) return std::string();

  if (!props.empty()) {
    if (!res.empty()) res += "\n  ";
    res += "properties {\n" + props + "}";
  }

  return res;
}


std::string JSObject::InspectElements(Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return std::string();

  FixedArray elements(elements_obj);

  Smi length_smi = elements.Length(err);
  if (err.Fail()) return std::string();

  int64_t length = length_smi.GetValue();
  return InspectElements(length, err);
}

int64_t JSObject::GetElementsLength(Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return 0;

  FixedArray elements(elements_obj);

  Smi length_smi = elements.Length(err);
  if (err.Fail()) return 0;

  return length_smi.GetValue();
}

elements_t* JSObject::InspectElementsX(Error& err, int64_t current,
                                       int64_t limit) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return nullptr;

  FixedArray elements(elements_obj);

  Smi length_smi = elements.Length(err);
  if (err.Fail()) return nullptr;

  int64_t length = length_smi.GetValue();
  return InspectElementsX(length, err, current, limit);
}


std::string JSObject::InspectElements(int64_t length, Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return std::string();
  FixedArray elements(elements_obj);

  InspectOptions options;

  std::string res;
  for (int64_t i = 0; i < length; i++) {
    Value value = elements.Get<Value>(i, err);
    if (err.Fail()) return std::string();

    bool is_hole = value.IsHole(err);
    if (err.Fail()) return std::string();

    // Skip holes
    if (is_hole) continue;

    if (!res.empty()) res += ",\n";

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "    [%d]=", static_cast<int>(i));
    res += tmp;

    res += value.Inspect(&options, err);
    if (err.Fail()) return std::string();
  }

  return res;
}

elements_t* JSObject::InspectElementsX(int64_t length, Error& err,
                                       int64_t current, int64_t limit) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return nullptr;
  FixedArray elements(elements_obj);

  InspectOptions options;

  int64_t start = current;
  if (start >= length) start = length;
  int64_t end = length;
  if (limit != 0) end = current + limit;
  if (end >= length) end = length;
  elements_t* elementstmp = new elements_t;
  inspect_t** element = new inspect_t*[end - start];
  elementstmp->length = static_cast<int>(end - start);
  elementstmp->elements = element;
  elementstmp->current = end;
  for (int64_t i = start; i < end; i++) {
    Value value = elements.Get<Value>(i, err);
    if (err.Fail()) {
      delete elementstmp;
      return nullptr;
    }

    bool is_hole = value.IsHole(err);

    if (err.Fail()) {
      delete elementstmp;
      return nullptr;
    }

    if (is_hole) {
      element[i - start] = nullptr;
      continue;
    }

    element[i - start] = value.InspectX(&options, err);
    if (err.Fail()) {
      delete elementstmp;
      return nullptr;
    }
  }

  return elementstmp;
}

std::string JSObject::InspectDictionary(Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return std::string();

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return std::string();

  InspectOptions options;

  std::string res;
  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);
    if (err.Fail()) return std::string();

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return std::string();
    if (is_hole) continue;

    Value value = dictionary.GetValue(i, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    res += "    ." + key.ToString(err) + "=";
    if (err.Fail()) return std::string();

    res += value.Inspect(&options, err);
    if (err.Fail()) return std::string();
  }

  return res;
}

int64_t JSObject::GetPropertiesLength(Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) return 0;
  Map map(map_obj);
  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return 0;
  if (is_dict) {
    HeapObject dictionary_obj = Properties(err);
    if (err.Fail()) return 0;

    NameDictionary dictionary(dictionary_obj);

    int64_t length = dictionary.Length(err);
    if (err.Fail()) return 0;
    return length;
  } else {
    HeapObject descriptors_obj = map.InstanceDescriptors(err);
    if (err.Fail()) return 0;

    DescriptorArray descriptors(descriptors_obj);
    int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
    if (err.Fail()) return 0;
    return own_descriptors_count;
  }
}

properties_t* JSObject::InspectDictionaryX(Error& err, int64_t current,
                                           int64_t limit) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return nullptr;

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (length < 0) length = 0;
  if (err.Fail()) return nullptr;

  InspectOptions options;

  int64_t start = current;
  if (start >= length) start = length;
  int64_t end = length;
  if (limit != 0) end = current + limit;
  if (end >= length) end = length;

  properties_t* properties = new properties_t;
  property_t** property = new property_t*[end - start];
  properties->length = static_cast<int>(end - start);
  properties->properties = property;
  properties->current = end;
  for (int64_t i = start; i < end; i++) {
    Value key = dictionary.GetKey(i, err);
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);

    if (err.Fail()) {
      delete properties;
      return nullptr;
    }

    if (is_hole) {
      property[i - start] = nullptr;
      continue;
    }

    Value value = dictionary.GetValue(i, err);
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }

    property_t* p = new property_t;
    property[i - start] = p;
    p->key = key.ToString(err);
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }

    p->value = value.InspectX(&options, err);
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }
  }

  return properties;
}


std::string JSObject::InspectDescriptors(Map map, Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (err.Fail()) return std::string();

  DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return std::string();

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return std::string();

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return std::string();

  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return std::string();

  FixedArray extra_properties(extra_properties_obj);

  InspectOptions options;

  std::string res;
  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i, err);
    if (err.Fail()) return std::string();

    Value key = descriptors.GetKey(i, err);
    if (err.Fail()) return std::string();

    if (!res.empty()) res += ",\n";

    res += "    ." + key.ToString(err) + "=";
    if (err.Fail()) return std::string();

    if (descriptors.IsConstFieldDetails(details) ||
        descriptors.IsDescriptorDetails(details)) {
      Value value;

      value = descriptors.GetValue(i, err);
      if (err.Fail()) return std::string();

      res += value.Inspect(&options, err);
      if (err.Fail()) return std::string();
      continue;
    }

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    if (descriptors.IsDoubleField(details)) {
      double value;
      if (index < 0)
        value = GetInObjectValue<double>(instance_size, index, err);
      else
        value = extra_properties.Get<double>(index, err);

      if (err.Fail()) return std::string();

      char tmp[100];
      snprintf(tmp, sizeof(tmp), "%f", value);
      res += tmp;
    } else {
      Value value;
      if (index < 0)
        value = GetInObjectValue<Value>(instance_size, index, err);
      else
        value = extra_properties.Get<Value>(index, err);

      if (err.Fail()) return std::string();

      res += value.Inspect(&options, err);
    }
    if (err.Fail()) return std::string();
  }

  return res;
}

properties_t* JSObject::InspectDescriptorsX(Map map, Error& err,
                                            int64_t current, int64_t limit) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (err.Fail()) return nullptr;

  DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (own_descriptors_count < 0) own_descriptors_count = 0;
  if (err.Fail()) return nullptr;

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return nullptr;

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return nullptr;

  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return nullptr;

  FixedArray extra_properties(extra_properties_obj);

  InspectOptions options;

  int64_t start = current;
  if (start >= own_descriptors_count) start = own_descriptors_count;
  int64_t end = own_descriptors_count;
  if (limit != 0) end = current + limit;
  if (end >= own_descriptors_count) end = own_descriptors_count;

  properties_t* properties = new properties_t;
  property_t** property = new property_t*[end - start];
  properties->length = static_cast<int>(end - start);
  properties->properties = property;
  properties->current = end;
  for (int64_t i = start; i < end; i++) {
    Smi details = descriptors.GetDetails(i, err);
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }

    Value key = descriptors.GetKey(i, err);
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }

    property_t* p = new property_t;
    property[i - start] = p;
    p->key = key.ToString(err);
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }

    if (descriptors.IsConstFieldDetails(details) ||
        descriptors.IsDescriptorDetails(details)) {
      Value value;

      value = descriptors.GetValue(i, err);
      if (err.Fail()) {
        delete properties;
        return nullptr;
      }

      p->value = value.InspectX(&options, err);
      if (err.Fail()) {
        delete properties;
        return nullptr;
      }
      continue;
    }

    // Skip non-fields for now
    if (!descriptors.IsFieldDetails(details)) {
      Error::PrintInDebugMode("Unknown field Type %" PRId64,
                              details.GetValue());
      p->value = nullptr;
      p->value_str = "unknown field type";
      continue;
    }

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    if (descriptors.IsDoubleField(details)) {
      double value;
      if (index < 0)
        value = GetInObjectValue<double>(instance_size, index, err);
      else
        value = extra_properties.Get<double>(index, err);

      if (err.Fail()) {
        delete properties;
        return nullptr;
      }

      char tmp[100];
      snprintf(tmp, sizeof(tmp), "%f", value);
      p->value = nullptr;
      p->value_str = tmp;
    } else {
      Value value;
      if (index < 0)
        value = GetInObjectValue<Value>(instance_size, index, err);
      else
        value = extra_properties.Get<Value>(index, err);

      if (err.Fail()) {
        delete properties;
        return nullptr;
      }

      p->value = value.InspectX(&options, err);
    }
    if (err.Fail()) {
      delete properties;
      return nullptr;
    }
  }

  return properties;
}


template <class T>
T JSObject::GetInObjectValue(int64_t size, int index, Error& err) {
  return LoadFieldValue<T>(size + index * v8()->common()->kPointerSize, err);
}


/* Returns the set of keys on an object - similar to Object.keys(obj) in
 * Javascript. That includes array indices but not special fields like
 * "length" on an array.
 */
void JSObject::Keys(std::vector<std::string>& keys, Error& err) {
  keys.clear();

  // First handle array indices.
  ElementKeys(keys, err);

  HeapObject map_obj = GetMap(err);

  Map map(map_obj);

  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return;

  if (is_dict) {
    DictionaryKeys(keys, err);
  } else {
    DescriptorKeys(keys, map, err);
  }

  return;
}


std::vector<std::pair<Value, Value>> JSObject::Entries(Error& err) {
  HeapObject map_obj = GetMap(err);

  Map map(map_obj);

  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return {};

  if (is_dict) {
    return DictionaryEntries(err);
  } else {
    return DescriptorEntries(map, err);
  }
}


std::vector<std::pair<Value, Value>> JSObject::DictionaryEntries(Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return {};

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return {};

  std::vector<std::pair<Value, Value>> entries;
  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);

    if (err.Fail()) return entries;

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return entries;
    if (is_hole) continue;

    Value value = dictionary.GetValue(i, err);

    entries.push_back(std::pair<Value, Value>(key, value));
  }
  return entries;
}


std::vector<std::pair<Value, Value>> JSObject::DescriptorEntries(Map map,
                                                                 Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (err.Fail()) return {};

  DescriptorArray descriptors(descriptors_obj);

  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return {};

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return {};

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return {};

  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return {};

  FixedArray extra_properties(extra_properties_obj);

  std::vector<std::pair<Value, Value>> entries;
  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i, err);
    if (err.Fail()) continue;

    Value key = descriptors.GetKey(i, err);
    if (err.Fail()) continue;

    if (descriptors.IsConstFieldDetails(details) ||
        descriptors.IsDescriptorDetails(details)) {
      Value value;

      value = descriptors.GetValue(i, err);
      if (err.Fail()) continue;

      entries.push_back(std::pair<Value, Value>(key, value));
      continue;
    }

    // Skip non-fields for now, Object.keys(obj) does
    // not seem to return these (for example the "length"
    // field on an array).
    if (!descriptors.IsFieldDetails(details)) continue;

    if (descriptors.IsDoubleField(details)) continue;

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    Value value;
    if (index < 0) {
      value = GetInObjectValue<Value>(instance_size, index, err);
    } else {
      value = extra_properties.Get<Value>(index, err);
    }

    entries.push_back(std::pair<Value, Value>(key, value));
  }

  return entries;
}


void JSObject::ElementKeys(std::vector<std::string>& keys, Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return;

  FixedArray elements(elements_obj);

  Smi length_smi = elements.Length(err);
  if (err.Fail()) return;

  int64_t length = length_smi.GetValue();
  for (int i = 0; i < length; ++i) {
    // Add keys for anything that isn't a hole.
    Value value = elements.Get<Value>(i, err);
    if (err.Fail()) continue;
    ;

    bool is_hole = value.IsHole(err);
    if (err.Fail()) continue;
    if (!is_hole) {
      keys.push_back(std::to_string(i));
    }
  }
}

void JSObject::DictionaryKeys(std::vector<std::string>& keys, Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return;

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return;

  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);
    if (err.Fail()) return;

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return;
    if (is_hole) continue;

    std::string key_name = key.ToString(err);
    if (err.Fail()) {
      // TODO - should I continue onto the next key here instead.
      return;
    }

    keys.push_back(key_name);
  }
}

void JSObject::DescriptorKeys(std::vector<std::string>& keys, Map map,
                              Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (err.Fail()) return;

  DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return;

  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i, err);
    if (err.Fail()) return;

    Value key = descriptors.GetKey(i, err);
    if (err.Fail()) return;

    // Skip non-fields for now, Object.keys(obj) does
    // not seem to return these (for example the "length"
    // field on an array).
    if (!descriptors.IsFieldDetails(details)) {
      continue;
    }

    std::string key_name = key.ToString(err);
    if (err.Fail()) {
      // TODO - should I continue onto the next key here instead.
      return;
    }

    keys.push_back(key_name);
  }
}

/* Return the v8 value for a property stored using the given key.
 * (Caller should have some idea of what type of object will be stored
 * in that key, they will get a v8::Value back that they can cast.)
 */
Value JSObject::GetProperty(std::string key_name, Error& err) {
  HeapObject map_obj = GetMap(err);
  if (err.Fail()) Value();

  Map map(map_obj);

  bool is_dict = map.IsDictionary(err);
  if (err.Fail()) return Value();

  if (is_dict) {
    return GetDictionaryProperty(key_name, err);
  } else {
    return GetDescriptorProperty(key_name, map, err);
  }

  if (err.Fail()) return Value();

  // Nothing's gone wrong, we just didn't find the key.
  return Value();
}

Value JSObject::GetDictionaryProperty(std::string key_name, Error& err) {
  HeapObject dictionary_obj = Properties(err);
  if (err.Fail()) return Value();

  NameDictionary dictionary(dictionary_obj);

  int64_t length = dictionary.Length(err);
  if (err.Fail()) return Value();

  for (int64_t i = 0; i < length; i++) {
    Value key = dictionary.GetKey(i, err);
    if (err.Fail()) return Value();

    // Skip holes
    bool is_hole = key.IsHoleOrUndefined(err);
    if (err.Fail()) return Value();
    if (is_hole) continue;

    if (key.ToString(err) == key_name) {
      Value value = dictionary.GetValue(i, err);

      if (err.Fail()) return Value();

      return value;
    }
  }
  return Value();
}

Value JSObject::GetDescriptorProperty(std::string key_name, Map map,
                                      Error& err) {
  HeapObject descriptors_obj = map.InstanceDescriptors(err);
  if (err.Fail()) return Value();

  DescriptorArray descriptors(descriptors_obj);
  int64_t own_descriptors_count = map.NumberOfOwnDescriptors(err);
  if (err.Fail()) return Value();

  int64_t in_object_count = map.InObjectProperties(err);
  if (err.Fail()) return Value();

  int64_t instance_size = map.InstanceSize(err);
  if (err.Fail()) return Value();

  HeapObject extra_properties_obj = Properties(err);
  if (err.Fail()) return Value();

  FixedArray extra_properties(extra_properties_obj);

  for (int64_t i = 0; i < own_descriptors_count; i++) {
    Smi details = descriptors.GetDetails(i, err);
    if (err.Fail()) return Value();

    Value key = descriptors.GetKey(i, err);
    if (err.Fail()) return Value();

    if (key.ToString(err) != key_name) {
      continue;
    }

    // Found the right key, get the value.
    if (err.Fail()) return Value();

    if (descriptors.IsConstFieldDetails(details) ||
        descriptors.IsDescriptorDetails(details)) {
      Value value;

      value = descriptors.GetValue(i, err);
      if (err.Fail()) return Value();

      continue;
    }

    // Skip non-fields for now
    if (!descriptors.IsFieldDetails(details)) {
      // This path would return the length field for an array,
      // however Object.keys(arr) doesn't return length as a
      // field so neither do we.
      continue;
    }

    int64_t index = descriptors.FieldIndex(details) - in_object_count;

    if (descriptors.IsDoubleField(details)) {
      double value;
      if (index < 0) {
        value = GetInObjectValue<double>(instance_size, index, err);
      } else {
        value = extra_properties.Get<double>(index, err);
      }

      if (err.Fail()) return Value();

    } else {
      Value value;
      if (index < 0) {
        value = GetInObjectValue<Value>(instance_size, index, err);
      } else {
        value = extra_properties.Get<Value>(index, err);
      }

      if (err.Fail()) {
        return Value();
      } else {
        return value;
      };
    }
    if (err.Fail()) return Value();
  }
  return Value();
}


/* An array is also an object so this method is on JSObject
 * not JSArray.
 */
int64_t JSObject::GetArrayLength(Error& err) {
  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return 0;

  FixedArray elements(elements_obj);
  Smi length_smi = elements.Length(err);
  if (err.Fail()) return 0;

  int64_t length = length_smi.GetValue();
  return length;
}


/* An array is also an object so this method is on JSObject
 * not JSArray.
 * Note that you the user should know what the expect the array to contain
 * and should check they haven't been returned a hole.
 */
v8::Value JSObject::GetArrayElement(int64_t pos, Error& err) {
  if (pos < 0) {
    // TODO - Set err.Fail()?
    return Value();
  }

  HeapObject elements_obj = Elements(err);
  if (err.Fail()) return Value();

  FixedArray elements(elements_obj);
  Smi length_smi = elements.Length(err);
  if (err.Fail()) return Value();

  int64_t length = length_smi.GetValue();
  if (pos >= length) {
    return Value();
  }

  Value value = elements.Get<v8::Value>(pos, err);
  if (err.Fail()) return Value();

  return value;
}

std::string JSArray::Inspect(InspectOptions* options, Error& err) {
  int64_t length = GetArrayLength(err);
  if (err.Fail()) return std::string();

  std::string res = "<Array: length=" + std::to_string(length);
  if (options->detailed) {
    int64_t display_length = std::min<int64_t>(length, options->length);
    std::string elems = InspectElements(display_length, err);
    if (err.Fail()) return std::string();

    if (!elems.empty()) res += " {\n" + elems + "}";
  }

  return res + ">";
}

js_array_t* JSArray::InspectX(InspectOptions* options, Error& err) {
  int64_t length = GetArrayLength(err);
  if (err.Fail()) return nullptr;

  js_array_t* js_array = new js_array_t;
  js_array->type = kJsArray;
  js_array->name = "Array";
  js_array->total_length = static_cast<int>(length);
  if (options->detailed) {
    // int64_t display_length = std::min<int64_t>(length, options->length);
    int option_current = options->current;
    if (option_current < 0) option_current = 0;
    int option_limit = options->limit;
    if (option_limit < 0) option_limit = 0;
    js_array->display_elemets =
        InspectElementsX(length, err, option_current, option_limit);
    if (err.Fail()) {
      delete js_array;
      return nullptr;
    }
  } else {
    js_array->display_elemets = nullptr;
  }

  return js_array;
}

}  // namespace v8
}  // namespace llnode
