#include "buffer.h"
#include "gzip.h"
#include "logger.h"

namespace atlas {
namespace util {

void Buffer::append(void* data, size_t data_size) {
  auto* p = static_cast<char*>(data);
  buf.insert(buf.end(), p, p + data_size);
}

void Buffer::assign(std::string* s) { s->assign(buf.begin(), buf.end()); }

void Buffer::uncompress_to(std::string* s) {
  auto res = inflate_string(s, buf.data(), buf.size());
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
                    buf.size(), err_msg);
  }
}

}  // namespace util
}  // namespace atlas
