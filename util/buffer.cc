#include "buffer.h"
#include "logger.h"
#include "gzip.h"

namespace atlas {
namespace util {

void Buffer::append(void* data, size_t data_size) {
  memory = static_cast<char*>(realloc(memory, size + data_size + 1));
  if (memory == nullptr) {
    throw std::bad_alloc();
  }

  memcpy(memory + size, data, data_size);
  size += data_size;
  memory[size] = 0;
}

void Buffer::assign(std::string* s) { s->assign(memory, size); }

void Buffer::uncompress_to(std::string* s) {
  auto res = inflate_string(s, memory, size);
  if (res != Z_OK) {
    std::string err_msg;
    switch (res) {
      case Z_MEM_ERROR:
        err_msg = "Out of memory";
        break;
      case Z_DATA_ERROR:
        err_msg = "Invalid or incomplete compressed data";
        break;
      case Z_STREAM_ERROR:
        err_msg = "Invalid compression level";
        break;
      default:
        err_msg = std::to_string(res);
    }
    Logger()->error("Unable to decompress payload (compressed size={}) err={}",
                    size, err_msg);
  }
  return;
}

}  // namespace util
}  // namespace atlas
