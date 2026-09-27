# Design Log — Project 2

## Growth factor and amortized cost

For my Conversation class, I picked a 2x doubling growth factor starting from capacity 0 and going to 1 on the first append (0 -> 1 -> 2 -> 4 -> 8 -> 16 ...).

I avoided adding a fixed number of slots (like +5 each time) because that hurts performance. Constant additions trigger reallocations every few appends, copying all elements repeatedly and turning N appends into an O(N^2) operation.

Doubling capacity gives amortized O(1) time per append. Here is the math for inserting N elements:
Every append takes 1 unit of work to place the message into the array. Resizing only happens when the array fills up at powers of 2. When resizing at capacity 2^j, we copy those 2^j elements into the new array.

Across N insertions, the total number of element copies is the sum of powers of 2 up to the largest power of 2 under N:
1 + 2 + 4 + ... + 2^k, where 2^k < N.
This geometric series sums to 2^(k+1) - 1, which is strictly less than 2N.

Adding the N direct insertions and at most 2N copies gives under 3N operations for N insertions:
Total work / N < 3N / N = 3 operations per append on average.
Since 3 is a constant, the amortized cost per append is O(1). Most appends are just an instant array write, easily paying for the occasional resize.

## Rule of Five evidence

Because Conversation uses a raw `Message* data_` pointer on the heap, I implemented all five special member functions to manage ownership and prevent memory leaks:

1. Destructor (~Conversation): Calls delete[] data*. If data* is nullptr (from an empty or moved-from object), delete[] handles it safely.
2. Copy constructor: Allocates a new buffer of other.capacity\_ and copies each Message over. This gives a deep copy where `this->begin() != other.begin()`. Mutating one conversation never affects the other.
3. Copy assignment: Checks for self-assignment (this == &other). It allocates a new buffer and copies messages first before deleting old data, so if allocation throws, our object stays intact without dangling pointers.
4. Move constructor: Marked noexcept. It steals other's data* pointer, size*, and capacity*, then zeroes out other (data* = nullptr, size* = 0, capacity* = 0). No allocations or copies needed.
5. Move assignment: Marked noexcept. It deletes the existing buffer, steals other's data\_ pointer and sizes, and leaves other in an empty valid state.

I verified this using AddressSanitizer (-fsanitize=address,undefined). My test suite checks pointer inequality on copies, confirms moved-from objects are zeroed out, and tests self-assignment. ASan reported 0 leaks.

## Sentinel scanner: bounded pending\_ proof

The SentinelScanner must catch `<|end_conversation|>` even when streamed in random chunks or 1 byte at a time, without buffering the whole conversation in memory.

I used a pending* string to hold trailing characters that might start the sentinel. Let M be the sentinel length (22 chars). My invariant is that pending* never exceeds M - 1 characters (21 chars max).

Here is why:

- At the start, pending\_ is empty (length 0 <= M - 1).
- On each feed(chunk), I concatenate pending\_ and chunk into a combined string of length L.
- If the sentinel is found in combined, we emit whatever came before it as safe text, clear pending\_, and set sentinel_found = true.
- If the sentinel is not found:
  1. If L < M, the text is shorter than the sentinel. It could all be the start of the sentinel, so safe text is empty and pending* becomes combined. Since L < M, pending*.size() <= M - 1 holds.
  2. If L >= M, a sentinel match could only start in the last M - 1 characters. Anything before that is safe to emit. So I output the first L - (M - 1) characters as safe*text and keep the trailing M - 1 characters in pending*. Thus pending\_.size() == M - 1 <= M - 1.

In all cases, pending* stays <= M - 1. I stress-tested this by feeding 4MB of data 1 byte at a time and asserting pending*.size() <= 21 on every byte.

## What I would design differently

In hindsight, I would change how Conversation allocates memory. Using `new Message[capacity_]` forces Message to have a default constructor that builds dummy empty System messages for unused slots. Every time capacity doubles, I construct empty strings that get overwritten later anyway.

It would be cleaner to allocate raw uninitialized bytes with `operator new` and use placement new to construct Message objects only when append() actually adds them. That avoids wasteful default constructions and works much more like std::vector.
