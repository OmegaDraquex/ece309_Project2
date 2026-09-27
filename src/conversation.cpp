// include/core/conversation.h
#pragma once

#include "core/message.h"
#include <cstddef>

class Conversation
{
public:
  // Empty conversation: size() == 0, no allocation yet.
  Conversation();

  // Releases all owned Message storage. No effect if already empty
  // (e.g. moved-from).
  ~Conversation();

  // Deep copy: allocates its own buffer and copies every Message.
  // this->begin() must differ from other.begin() afterward.
  Conversation(const Conversation &other);
  Conversation &operator=(const Conversation &other);

  // Steals other's buffer — no per-element copying. Afterward, other
  // must be left valid and empty (safe to destroy or reassign).
  Conversation(Conversation &&other) noexcept;
  Conversation &operator=(Conversation &&other) noexcept;

  // Appends m, growing the backing array if needed. Amortized O(1).
  void append(Message m);

  // Number of messages currently stored.
  std::size_t size() const noexcept;

  // Capacity of the backing array.
  std::size_t capacity() const noexcept;

  // Bounds-checked access. Throws std::out_of_range on i >= size().
  const Message &at(std::size_t i) const;
  Message &at(std::size_t i);

  // Range-for iteration, oldest message first. begin() == end() when size() == 0.
  const Message *begin() const noexcept;
  const Message *end() const noexcept;

private:
  Message *data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;
};
