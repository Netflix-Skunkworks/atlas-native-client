#pragma once

#include <zlib.h>

namespace atlas {
namespace util {

static constexpr int kGzipHeaderSize = 16;  // size of the gzip header

int gzip_compress(Bytef* dest, uLongf* destLen, const Bytef* source,
                  uLong sourceLen);
int gzip_uncompress(Bytef* dest, uLongf* destLen, const Bytef* source,
                    uLong sourceLen);
}  // namespace util
}  // namespace atlas
