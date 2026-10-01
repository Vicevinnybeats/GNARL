// GNARL: the browser build's stand-in for JuceHeader.h (wasm/README.md).
// The engine uses JUCE for a leak detector and nothing on its audio path, so
// the web build compiles it without JUCE. What else it needs is added here
// only as compile errors ask for it.
#pragma once

#define JUCE_LEAK_DETECTOR(Class)
#define JUCE_DECLARE_NON_COPYABLE(Class)
#define jassert(x) ((void)0)
#define jassertfalse ((void)0)
#define JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Class)

// JuceHeader.h brought these in transitively; the engine relies on that.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Declarations only: tuning.h names these in signatures the web build never
// calls (it compiles no tuning file loader).
class File;
class StringArray;

// Just enough for Sample::stateToJson / jsonToState (sample_source.cpp),
// which base64-encode a sample's PCM in a preset. Standard base64, as JUCE's.
class String {
  public:
    String() = default;
    String(std::string s) : s_(std::move(s)) { }
    String(const char* s) : s_(s) { }
    std::string toStdString() const { return s_; }
  private:
    std::string s_;
};

class MemoryOutputStream {
  public:
    explicit MemoryOutputStream(size_t reserve = 0) { data_.reserve(reserve); }
    void write(const void* data, size_t size) {
      const char* bytes = static_cast<const char*>(data);
      data_.insert(data_.end(), bytes, bytes + size);
    }
    const void* getData() const { return data_.data(); }
    size_t getDataSize() const { return data_.size(); }
  private:
    std::vector<char> data_;
};

struct Base64 {
  static String toBase64(const void* data, size_t size) {
    static const char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char* in = static_cast<const unsigned char*>(data);
    std::string out;
    out.reserve(4 * ((size + 2) / 3));
    for (size_t i = 0; i < size; i += 3) {
      unsigned int n = in[i] << 16;
      if (i + 1 < size) n |= in[i + 1] << 8;
      if (i + 2 < size) n |= in[i + 2];
      out += kChars[(n >> 18) & 63];
      out += kChars[(n >> 12) & 63];
      out += i + 1 < size ? kChars[(n >> 6) & 63] : '=';
      out += i + 2 < size ? kChars[n & 63] : '=';
    }
    return String(out);
  }

  static bool convertFromBase64(MemoryOutputStream& out, const std::string& text) {
    unsigned int buffer = 0;
    int bits = 0;
    for (char c : text) {
      int v;
      if (c >= 'A' && c <= 'Z') v = c - 'A';
      else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
      else if (c >= '0' && c <= '9') v = c - '0' + 52;
      else if (c == '+') v = 62;
      else if (c == '/') v = 63;
      else continue;
      buffer = (buffer << 6) | v;
      bits += 6;
      if (bits >= 8) {
        bits -= 8;
        char byte = static_cast<char>((buffer >> bits) & 0xff);
        out.write(&byte, 1);
      }
    }
    return true;
  }
};

// WebAssembly has no flush-to-zero mode to switch on: its float semantics are
// fixed IEEE 754, denormals included. See wasm/README.md.
struct FloatVectorOperations {
  static void disableDenormalisedNumberSupport() { }
};
