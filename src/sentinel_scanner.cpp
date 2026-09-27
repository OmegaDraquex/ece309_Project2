#include "core/sentinel_scanner.h"
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)), pending_("") {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk)
{
  std::string combined = pending_;
  combined.append(chunk.data(), chunk.size());

  // Search for sentinel in the combined window
  std::size_t pos = combined.find(sentinel_);
  if (pos != std::string::npos)
  {
    std::string safe = combined.substr(0, pos);
    pending_.clear();
    return Out{std::move(safe), true};
  }

  if (combined.size() >= sentinel_.size())
  {
    std::size_t keep = sentinel_.size() - 1;
    std::size_t safe_length = combined.size() - keep;
    std::string safe = combined.substr(0, safe_length);
    pending_ = combined.substr(safe_length);
    return Out{std::move(safe), false};
  }
  else
  {
    pending_ = std::move(combined);
    return Out{"", false};
  }
}

SentinelScanner::Out SentinelScanner::flush()
{
  std::string safe = std::move(pending_);
  pending_.clear();
  return Out{std::move(safe), false};
}
