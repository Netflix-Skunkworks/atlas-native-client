#include "validation.h"
#include "id_format.h"

#include "../util/logger.h"
#include "../util/strings.h"
#include <unordered_set>

namespace atlas {
namespace meter {

static constexpr size_t MAX_KEY_LENGTH = 60;
static constexpr size_t MAX_VAL_LENGTH = 120;
static constexpr size_t MAX_USER_TAGS = 20;
static constexpr size_t MAX_NAME_LENGTH = 255;

using util::intern_str;
using util::StartsWith;
using util::StrRef;

static bool is_key_restricted(StrRef k) noexcept {
  return StartsWith(k, "nf.") || StartsWith(k, "atlas.");
}

static const std::unordered_set<StrRef>& valid_nf_tags() {
  static std::unordered_set<StrRef> kValidNfTags = {
      intern_str("nf.node"),          intern_str("nf.cluster"),
      intern_str("nf.app"),           intern_str("nf.asg"),
      intern_str("nf.stack"),         intern_str("nf.ami"),
      intern_str("nf.vmtype"),        intern_str("nf.zone"),
      intern_str("nf.region"),        intern_str("nf.account"),
      intern_str("nf.country"),       intern_str("nf.task"),
      intern_str("nf.country.rollup")};

  return kValidNfTags;
}

static bool is_user_key_invalid(StrRef k) noexcept {
  static auto kDsType = intern_str("atlas.dstype");
  static auto kLegacy = intern_str("atlas.legacy");

  if (StartsWith(k, "atlas.")) {
    return k != kDsType && k != kLegacy;
  }

  auto& valid_tags = valid_nf_tags();
  if (StartsWith(k, "nf.")) {
    return valid_tags.find(k) == valid_tags.end();
  }

  return false;
}

static bool empty_or_null(StrRef r) { return r.is_null() || r.length() == 0; }

bool AreTagsValid(const Tags& tags) noexcept {
  static auto kNameRef = intern_str("name");

  std::string err_msg;
  auto user_tags = 0u;
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
        err_msg = fmt::format("value for name exceeds length limit ({} > {})",
                              v_length, MAX_NAME_LENGTH);
        goto invalid;
      }
      continue;
    }

    auto k_length = k_ref.length();
    auto v_length = v_ref.length();
    if (k_length > MAX_KEY_LENGTH || v_length > MAX_VAL_LENGTH) {
      err_msg = fmt::format("Tag {}={} exceeds length limits ({}-{}, {}-{})",
                            k_ref.get(), v_ref.get(), k_length, MAX_KEY_LENGTH,
                            v_length, MAX_VAL_LENGTH);
      goto invalid;
    }

    if (!is_key_restricted(k_ref)) {
      ++user_tags;
    }

    if (is_user_key_invalid(k_ref)) {
      err_msg = fmt::format("{} is using a reserved namespace", k_ref.get());
      goto invalid;
    }
  }

  if (user_tags > MAX_USER_TAGS) {
    err_msg = fmt::format(
        "Too many user tags. There is a {} user tags limit. Detected user tags "
        "= {})",
        MAX_USER_TAGS, user_tags);
    goto invalid;
  }

  // Needs a name
  if (!name_seen) {
    err_msg = "name is a required tag";
    goto invalid;
  }

  return true;

invalid:
  logger->warn("Invalid metric {} - {}", tags, err_msg);
  return false;
}

ValidationIssues AnalyzeTags(const Tags& tags) noexcept {
  static auto kNameRef = intern_str("name");
  auto name_seen = false;
  auto user_tags = 0u;

  ValidationIssues result;
  for (const auto& kv : tags) {
    const auto& k_ref = kv.first;
    const auto& v_ref = kv.second;

    // check charset
    auto validated_key = util::ToValidCharset(k_ref);
    if (validated_key != k_ref) {
      result.emplace(
          ValidationIssue::Warn(fmt::format("Key: '{}' will be encoded as '{}'",
                                            k_ref.get(), validated_key.get())));
    }
    auto validated_val = util::EncodeValueForKey(v_ref, k_ref);
    if (validated_val != v_ref) {
      result.emplace(ValidationIssue::Warn(
          fmt::format("Value: '{}' for key '{}' will be encoded as '{}'",
                      v_ref.get(), k_ref.get(), validated_val.get())));
    }

    if (empty_or_null(k_ref) && empty_or_null(v_ref)) {
      result.emplace(ValidationIssue::Err(
          "Tags cannot be empty. Found empty key and value."));
    } else if (empty_or_null(k_ref)) {
      result.emplace(ValidationIssue::Err(
          fmt::format("Empty key found with value '{}'", v_ref.get())));
    } else if (empty_or_null(v_ref)) {
      result.emplace(ValidationIssue::Err(
          fmt::format("Tag value for key '{}' cannot be empty", k_ref.get())));
    }

    if (k_ref == kNameRef) {
      name_seen = true;
      ++user_tags;
      auto v_length = v_ref.length();
      if (v_length > MAX_NAME_LENGTH) {
        result.emplace(ValidationIssue::Err(
            fmt::format("value for name exceeds length limit ({} > {})",
                        v_length, MAX_NAME_LENGTH)));
      }
    } else {
      auto k_length = k_ref.length();
      auto v_length = v_ref.length();
      if (k_length > MAX_KEY_LENGTH) {
        result.emplace(ValidationIssue::Err(
            fmt::format("Tag key '{}' exceeds length limits ({}-{})",
                        k_ref.get(), k_length, MAX_KEY_LENGTH)));
      }
      if (v_length > MAX_VAL_LENGTH) {
        result.emplace(ValidationIssue::Err(
            fmt::format("Tag value '{}' exceeds length limits ({}-{})",
                        v_ref.get(), v_length, MAX_VAL_LENGTH)));
      }

      if (!is_key_restricted(k_ref)) {
        ++user_tags;
      }

      if (is_user_key_invalid(k_ref)) {
        result.emplace(ValidationIssue::Err(fmt::format(
            "Tag key '{}' is using a reserved namespace", k_ref.get())));
      }
    }
  }

  if (user_tags > MAX_USER_TAGS) {
    result.emplace(
        ValidationIssue::Err(fmt::format("Too many user tags. There is a limit "
                                         "of {} user tags. Detected user tags "
                                         "= {}",
                                         MAX_USER_TAGS, user_tags)));
  }

  if (!name_seen) {
    result.emplace(ValidationIssue::Err("name is a required tag"));
  }
  return result;
}

ValidationIssue ValidationIssue::Err(std::string message) noexcept {
  ValidationIssue res;
  res.level = ValidationIssue::Level::ERROR;
  res.description = std::move(message);
  return res;
}

ValidationIssue ValidationIssue::Warn(std::string message) noexcept {
  ValidationIssue res;
  res.level = ValidationIssue::Level::WARN;
  res.description = std::move(message);
  return res;
}

bool ValidationIssue::operator<(const ValidationIssue& rhs) const noexcept {
  if (level < rhs.level) {
    return true;
  }
  if (rhs.level < level) {
    return false;
  }
  return description < rhs.description;
}

std::string ValidationIssue::ToString() const noexcept {
  return fmt::format("{}: {}", level == Level::WARN ? "WARN" : "ERROR",
                     description);
}

}  // namespace meter
}  // namespace atlas
