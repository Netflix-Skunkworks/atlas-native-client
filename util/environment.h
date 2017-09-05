#pragma once

#include "../meter/id.h"

namespace atlas {
namespace util {

std::string safe_getenv(const char* var) noexcept;

namespace env {

std::string ami() noexcept;
std::string app() noexcept;
std::string asg() noexcept;
std::string instance_id() noexcept;
std::string cluster() noexcept;
std::string stack() noexcept;
std::string vmtype() noexcept;
std::string taskid() noexcept;
std::string region() noexcept;
std::string account() noexcept;
std::string zone() noexcept;

}  // namespace env
}  // namespace util
}  // namespace atlas
