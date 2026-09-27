#pragma once

#include <string>
#include <string_view>
#include <cstddef>

class SentinelScanner
{
public:
  explicit SentinelScanner(std::string sentinel);

  struct Out
  {
    std::string safe_text;
    bool sentinel_found;
  };

  // Feed the next chunk. Returns text guaranteed NOT to be part of
  // the sentinel (safe to print immediately) and whether the
  // sentinel has now been fully seen.
  Out feed(std::string_view chunk);

  // Call once,after the stream ends, to release any text still
  // being held back.
  Out flush();

  // Helper accessor for test assertions on memory bounds:
  std::size_t pending_size() const noexcept
  {
    return pending_.size();
  }

private:
  std::string sentinel_;
  std::string pending_;
};
