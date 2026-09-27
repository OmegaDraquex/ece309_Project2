# ECE 309: Project 2 — MiniHarness

`miniharness` is an execution harness that bridges language model backends and interactive user sessions. It manages conversation memory, enforces turn limits, handles graceful termination upon detecting stop sentinels in arbitrary streaming chunk sequences, and records reproducible session transcripts.

---

## Architecture & Components

The codebase consists of:

1. **`Message` (`include/core/message.h`)**:
   - Encapsulates conversation turns with a `Role` (`System`, `User`, `Assistant`) and string `content`.
   - Default-constructs an empty `System` message to enable raw dynamic array allocation.

2. **`Conversation` (`include/core/conversation.h`, `src/conversation.cpp`)**:
   - Custom growable array managing heap memory via raw pointers (`Message* data_`, `size_`, `capacity_`) without `std::vector`.
   - Implements the complete **Rule of Five** (destructor, deep copy constructor/assignment, and pointer-stealing move constructor/assignment).
   - Features a $2\times$ geometric growth factor ensuring amortized $O(1)$ appends and bounds-checked access throwing `std::out_of_range`.

3. **`SentinelScanner` (`include/core/sentinel_scanner.h`, `src/sentinel_scanner.cpp`)**:
   - Detects the stop sentinel (`<|end_conversation|>`) across arbitrary-length streaming token chunks.
   - Maintains an $O(1)$ bounded trailing buffer `pending_` ($\le |\text{sentinel}| - 1$ characters), releasing verified safe text immediately without buffering entire model responses.

4. **Provided Harness & Model Clients (`include/harness/`, `include/model/`, `src/`)**:
   - `Harness`: Coordinates the execution loop between user input, model streaming generation, scanner filtering, conversation history updates, and turn tracking.
   - `ScriptedModelClient`: Generates scripted responses from `.script` files, supporting variable chunking directives.
   - `ReplayModelClient`: Loads recorded transcripts and replays assistant turns verbatim for deterministic verification.

---

## Build Instructions

The project requires CMake 3.16+ and a C++17 compliant compiler (`g++` or `clang++`). AddressSanitizer and UndefinedBehaviorSanitizer are enabled by default in `CMakeLists.txt` (`-fsanitize=address,undefined`).

```bash
# Configure the build directory
cmake -S . -B build

# Build both miniharness and the test suite
cmake --build build
```

This generates two executables in `./build/`:

- `miniharness`: The interactive conversation CLI.
- `test_p2`: The automated unit and integration test suite.

---

## Running MiniHarness

### Command-Line Arguments

- `--script <path>`: Path to a scripted dialog file (default: `default.script`).
- `--max-turns <N>`: Maximum number of conversation turns before halting with `TurnLimit` (default: `20`).
- `--save <path>`: Writes the complete conversation transcript to disk upon exit.

### Interactive Execution

```bash
# Run with the default greeting script
./build/miniharness --script scripts/greeting.script

# Run with a turn limit and save the transcript
./build/miniharness --script scripts/greeting.script --max-turns 5 --save transcript.txt
```

### Clean Exit via EOF (Ctrl-D)

When standard input reaches EOF (e.g., pressing `Ctrl-D` on an empty line in Unix/Linux or piped input completion), `miniharness` terminates gracefully with `[conversation ended: EOF detected]` and still writes out the saved transcript if `--save` was specified.

---

## Running the Test Suite

Run the test suite to execute all 12 comprehensive unit and integration test cases:

```bash
./build/test_p2
```

The test suite validates:

1. Empty conversation bounds checking (`std::out_of_range` on invalid indices).
2. System message pinning and chronological turn ordering.
3. Rule of Five copy semantics (deep copies and distinct buffer addresses).
4. Rule of Five move semantics (pointer stealing, resource nullification).
5. Geometric array growth and element preservation across reallocations.
6. Clean stream scanning and chunk reconstitution.
7. Sentinel detection across every possible integer split boundary.
8. Sentinel scanner rejection of false alarm near-matches.
9. Bounded memory consumption during 4MB streaming sequences.
10. Turn limit termination under the `Harness` loop.
11. Sentinel halting and terminal sanitization under the `Harness` loop.
12. End-to-end transcript saving and replay round-tripping.
