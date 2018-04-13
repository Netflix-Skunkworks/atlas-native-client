#pragma once

#include <string>
namespace atlas {
namespace util {

class Buffer {
 public:
  Buffer() : memory(static_cast<char*>(malloc(1))) {}
  ~Buffer() { free(memory); }
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  Buffer& operator=(Buffer&&) = delete;
  Buffer(Buffer&&) = delete;
  void append(void* data, size_t data_size);
  void assign(std::string* s);
  void uncompress_to(std::string* s);

 private:
  char* memory;
  size_t size = 0;
};

}  // namespace util
}  // namespace atlas
