#include "validation.h"
#include "../util/dump.h"
#include "../util/logger.h"
#include "../util/strings.h"
#include <sstream>
#include <unordered_set>

namespace atlas {
namespace meter {
namespace validation {

static constexpr size_t MAX_KEY_LENGTH = 60;
static constexpr size_t MAX_VAL_LENGTH = 120;
static constexpr size_t MAX_USER_TAGS = 20;
static constexpr size_t MAX_NAME_LENGTH = 255;

using util::StartsWith;

static bool is_key_restricted(const std::string& k) noexcept {
  return StartsWith(k, "nf.") || StartsWith(k, "atlas.");
}

static std::unordered_set<std::string> kValidNfTags = {
    "nf.node",    "nf.cluster", "nf.app",           "nf.asg",    "nf.stack",
    "nf.ami",     "nf.vmtype",  "nf.zone",          "nf.region", "nf.account",
    "nf.country", "nf.task",    "nf.country.rollup"};

static bool is_user_key_invalid(const std::string& k) noexcept {
  if (StartsWith(k, "atlas.")) {
    return k != "atlas.dstype" && k != "atlas.legacy";
  }

  if (StartsWith(k, "nf.")) {
    return kValidNfTags.find(k) == kValidNfTags.end();
  }

  return false;
}

bool IsValid(const Tags& tags) noexcept {
  std::string err_msg;
  size_t user_tags = 0;
  auto logger = util::Logger();
  auto name_seen = false;

  for (const auto& kv : tags) {
    const auto& k_ref = kv.first;
    const auto& v_ref = kv.second;
    const std::string k = k_ref.get();
    const std::string v = v_ref.get();

    if (k.empty() || v.empty()) {
      err_msg = "Tag keys or values cannot be empty";
      goto invalid;
    }

    if (k == "name") {
      name_seen = true;
      ++user_tags;
      if (v.length() > MAX_NAME_LENGTH) {
        std::ostringstream os;
        os << "value for name exceeds length limit (" << v.length() << ">"
           << MAX_NAME_LENGTH << ")";
        err_msg = os.str();
        goto invalid;
      }
      continue;
    }

    if (k.length() > MAX_KEY_LENGTH || v.length() > MAX_VAL_LENGTH) {
      std::ostringstream os;
      os << "Tag " << k << "=" << v << " exceeds length limits (" << k.length()
         << "-" << MAX_KEY_LENGTH << ", " << v.length() << "-" << MAX_VAL_LENGTH
         << ")";
      err_msg = os.str();
      goto invalid;
    }

    if (!is_key_restricted(k)) {
      ++user_tags;
    }

    if (is_user_key_invalid(k)) {
      err_msg = k + " is using a reserved namespace";
      goto invalid;
    }
  }

  if (user_tags > MAX_USER_TAGS) {
    std::ostringstream os;
    os << "Too many user tags. There is a " << MAX_USER_TAGS
       << " user tags limit. Detected user tags = " << user_tags;
    err_msg = os.str();
    goto invalid;
  }

  // Needs a name
  if (!name_seen) {
    err_msg = "name is a required tag";
    goto invalid;
  }

  return true;

invalid:
  std::ostringstream os;
  dump_tags(os, tags);
  logger->warn("Invalid metric {} - {}", os.str(), err_msg);
  return false;
}

}  // namespace validation
}  // namespace meter
}  // namespace atlas
