#pragma once

#include <string>
#include <vector>
namespace atlas {
namespace util {

class Buffer {
 public:
  Buffer() = default;
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;
  Buffer& operator=(Buffer&&) = delete;
  Buffer(Buffer&&) = delete;
  void append(void* data, size_t data_size);
  void assign(std::string* s);
  void uncompress_to(std::string* s);

 private:
  std::vector<char> buf;
};

}  // namespace util
}  // namespace atlas
