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

using util::intern_str;
using util::StartsWith;
using util::StrRef;

static bool is_key_restricted(util::StrRef k) noexcept {
  return StartsWith(k, "nf.") || StartsWith(k, "atlas.");
}

static std::unordered_set<util::StrRef> kValidNfTags = {
    intern_str("nf.node"),          intern_str("nf.cluster"),
    intern_str("nf.app"),           intern_str("nf.asg"),
    intern_str("nf.stack"),         intern_str("nf.ami"),
    intern_str("nf.vmtype"),        intern_str("nf.zone"),
    intern_str("nf.region"),        intern_str("nf.account"),
    intern_str("nf.country"),       intern_str("nf.task"),
    intern_str("nf.country.rollup")};

static auto kDsType = intern_str("atlas.dstype");
static auto kLegacy = intern_str("atlas.legacy");
static auto kNameRef = intern_str("name");

static bool is_user_key_invalid(util::StrRef k) noexcept {
  if (StartsWith(k, "atlas.")) {
    return k != kDsType && k != kLegacy;
  }

  if (StartsWith(k, "nf.")) {
    return kValidNfTags.find(k) == kValidNfTags.end();
  }

  return false;
}

bool empty_or_null(util::StrRef r) { return r.empty() || r.length() == 0; }

bool IsValid(const Tags& tags) noexcept {
  std::string err_msg;
  size_t user_tags = 0;
  auto logger = util::Logger();
  auto name_seen = false;

  for (const auto& kv : tags) {
    const auto& k_ref = kv.first;
    const auto& v_ref = kv.second;

    if (empty_or_null(k_ref) || empty_or_null(v_ref)) {
      err_msg = "Tag keys or values cannot be empty";
      goto invalid;
    }

    if (k_ref == kNameRef) {
      name_seen = true;
      ++user_tags;
      auto v_length = v_ref.length();
      if (v_length > MAX_NAME_LENGTH) {
        std::ostringstream os;
        os << "value for name exceeds length limit (" << v_length << ">"
           << MAX_NAME_LENGTH << ")";
        err_msg = os.str();
        goto invalid;
      }
      continue;
    }

    auto k_length = k_ref.length();
    auto v_length = v_ref.length();
    if (k_length > MAX_KEY_LENGTH || v_length > MAX_VAL_LENGTH) {
      std::ostringstream os;
      os << "Tag " << k_ref.get() << "=" << v_ref.get()
         << " exceeds length limits (" << k_length << "-" << MAX_KEY_LENGTH
         << ", " << v_length << "-" << MAX_VAL_LENGTH << ")";
      err_msg = os.str();
      goto invalid;
    }

    if (!is_key_restricted(k_ref)) {
      ++user_tags;
    }

    if (is_user_key_invalid(k_ref)) {
      err_msg = k_ref.get();
      err_msg += " is using a reserved namespace";
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
