#pragma once

#include <zlib.h>

namespace atlas {
namespace util {

int gzip_compress(Bytef* dest, uLongf* destLen, const Bytef* source,
                  uLong sourceLen);
int gzip_uncompress(Bytef* dest, uLongf* destLen, const Bytef* source,
                    uLong sourceLen);
}
}
