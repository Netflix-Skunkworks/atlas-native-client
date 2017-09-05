#include "environment.h"
#include "strings.h"
#include <sstream>
#include <unistd.h>

namespace atlas {
namespace util {

static std::string kEmptyString;

std::string safe_getenv(const char* var) noexcept {
  auto res = std::getenv(var);
  if (res == nullptr) {
    return kEmptyString;
  }
  return std::string{res};
}

namespace env {

static const char* const AMI = "EC2_AMI_ID";
static const char* const APP = "NETFLIX_APP";
static const char* const ASG = "NETFLIX_AUTO_SCALE_GROUP";
static const char* const CLUSTER = "NETFLIX_CLUSTER";
static const char* const STACK = "NETFLIX_STACK";
static const char* const INSTANCE_ID = "EC2_INSTANCE_ID";
static const char* const OWNER = "EC2_OWNER_ID";
static const char* const REGION = "EC2_REGION";
static const char* const TITUS_INSTANCE_ID = "TITUS_TASK_INSTANCE_ID";
static const char* const VM_TYPE = "EC2_INSTANCE_TYPE";
static const char* const ZONE = "EC2_AVAILABILITY_ZONE";
static const char* const DC_REGION = "us-nflx-1";
static const char* const DC_ZONE = "us-nflx-1a";
static const char* const TASK_ID = "TITUS_TASK_ID";

std::string instance_id() noexcept {
  auto v = first_nonempty(
      {safe_getenv(TITUS_INSTANCE_ID), safe_getenv(INSTANCE_ID)});
  if (v.empty()) {
    char hostname[1024];
    hostname[1023] = '\0';
    auto h = &hostname[0];
    if (gethostname(h, 1023) != 0) {
      std::stringstream os;
      os << "error-" << errno;
      return os.str();
    }
    return std::string{h};
  }
  return v;
}

std::string account() noexcept {
  return first_nonempty({safe_getenv(OWNER), "dc"});
}

std::string app() noexcept { return safe_getenv(APP); }

std::string cluster() noexcept { return safe_getenv(CLUSTER); }

std::string stack() noexcept { return safe_getenv(STACK); }

std::string ami() noexcept { return safe_getenv(AMI); }

std::string asg() noexcept { return safe_getenv(ASG); }

std::string vmtype() noexcept { return safe_getenv(VM_TYPE); }

std::string taskid() noexcept { return safe_getenv(TASK_ID); }

std::string zone() noexcept {
  return first_nonempty({safe_getenv(ZONE), DC_ZONE});
}

std::string region() noexcept {
  return first_nonempty({safe_getenv(REGION), DC_REGION});
}

}  // namespace env
}  // namespace util
}  // namespace atlas
