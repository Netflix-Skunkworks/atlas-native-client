#include "gzip.h"

#ifndef z_const
#define z_const
#endif

namespace atlas {
namespace util {

static constexpr int kWindowBits =
    15 + kGzipHeaderSize;            // 32k window (max size)
static constexpr int kMemLevel = 9;  // max memory for optimal speed

int gzip_compress(Bytef* dest, uLongf* destLen, const Bytef* source,
                  uLong sourceLen) {
  // no initialization due to gcc 4.8 bug
  z_stream stream;

  stream.next_in = const_cast<z_const Bytef*>(source);
  stream.avail_in = static_cast<uInt>(sourceLen);
  stream.next_out = dest;
  stream.avail_out = static_cast<uInt>(*destLen);
  stream.zalloc = static_cast<alloc_func>(nullptr);
  stream.zfree = static_cast<free_func>(nullptr);
  stream.opaque = static_cast<voidpf>(nullptr);

  auto err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                          kWindowBits, kMemLevel, Z_DEFAULT_STRATEGY);
  if (err != Z_OK) {
    return err;
  }

  err = deflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END) {
    deflateEnd(&stream);
    return err == Z_OK ? Z_BUF_ERROR : err;
  }
  *destLen = stream.total_out;

  return deflateEnd(&stream);
}

int gzip_uncompress(Bytef* dest, uLongf* destLen, const Bytef* source,
                    uLong sourceLen) {
  // no initialization due to gcc 4.8 bug
  z_stream stream;
  stream.next_in = const_cast<z_const Bytef*>(source);
  stream.avail_in = static_cast<uInt>(sourceLen);
  stream.next_out = dest;
  stream.avail_out = static_cast<uInt>(*destLen);
  stream.zalloc = static_cast<alloc_func>(nullptr);
  stream.zfree = static_cast<free_func>(nullptr);

  auto err = inflateInit2(&stream, kWindowBits);
  if (err != Z_OK) {
    return err;
  }

  err = inflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END) {
    inflateEnd(&stream);
    if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0)) {
      return Z_DATA_ERROR;
    }
    return err;
  }
  *destLen = stream.total_out;

  return inflateEnd(&stream);
}
}  // namespace util
}  // namespace atlas
