// tests/p2/test_p2.cpp
#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdio>

namespace
{

    class MockInputSource : public InputSource
    {
    public:
        explicit MockInputSource(std::vector<std::string> lines)
            : lines_(std::move(lines)) {}

        std::string read_line() override
        {
            if (index_ < lines_.size())
            {
                return lines_[index_++];
            }
            eof_ = true;
            return "";
        }

        bool is_eof() const override
        {
            return eof_;
        }

    private:
        std::vector<std::string> lines_;
        std::size_t index_ = 0;
        bool eof_ = false;
    };

    class MockOutputSink : public OutputSink
    {
    public:
        void write(std::string_view text) override
        {
            output_.append(text.data(), text.size());
        }

        const std::string &output() const
        {
            return output_;
        }

    private:
        std::string output_;
    };

    const char *role_name(Role role)
    {
        switch (role)
        {
        case Role::System:
            return "system";
        case Role::User:
            return "user";
        case Role::Assistant:
            return "assistant";
        }
        return "assistant";
    }

    void save_transcript(const Conversation &conv, const std::string &path)
    {
        std::ofstream file(path);
        assert(file.is_open());
        bool first = true;
        for (const Message *m = conv.begin(); m != conv.end(); ++m)
        {
            if (!first)
                file << "---\n";
            first = false;
            file << "role: " << role_name(m->role()) << "\n";
            file << m->content() << "\n";
        }
    }

} // namespace

// 1. Empty Conversation Bounds
void test_empty_conversation_bounds()
{
    Conversation conv;
    assert(conv.size() == 0);
    assert(conv.begin() == conv.end());

    bool threw = false;
    try
    {
        (void)conv.at(0);
    }
    catch (const std::out_of_range &)
    {
        threw = true;
    }
    assert(threw && "Calling at(0) on an empty conversation must throw std::out_of_range");

    threw = false;
    try
    {
        (void)conv.at(10);
    }
    catch (const std::out_of_range &)
    {
        threw = true;
    }
    assert(threw && "Calling at(10) on an empty conversation must throw std::out_of_range");
    std::cout << "[PASS] Test 1: Empty Conversation Bounds\n";
}

// 2. System Message Ordering
void test_system_message_ordering()
{
    Conversation conv;
    conv.append(Message(Role::System, "You are a helpful assistant."));
    conv.append(Message(Role::User, "Hello there!"));
    conv.append(Message(Role::Assistant, "Hello! How can I help you?"));

    assert(conv.size() == 3);
    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "You are a helpful assistant.");
    assert(conv.at(1).role() == Role::User);
    assert(conv.at(1).content() == "Hello there!");
    assert(conv.at(2).role() == Role::Assistant);
    assert(conv.at(2).content() == "Hello! How can I help you?");
    std::cout << "[PASS] Test 2: System Message Ordering\n";
}

// 3. Rule of Five (Copy)
void test_rule_of_five_copy()
{
    Conversation conv1;
    conv1.append(Message(Role::User, "Msg 1"));
    conv1.append(Message(Role::Assistant, "Msg 2"));

    // Copy constructor
    Conversation conv2 = conv1;
    assert(conv1.size() == conv2.size());
    assert(conv1.begin() != conv2.begin() && "Copy constructor must perform a deep copy with distinct buffer addresses");
    assert(conv2.at(0).content() == "Msg 1");
    assert(conv2.at(1).content() == "Msg 2");

    // Copy assignment
    Conversation conv3;
    conv3.append(Message(Role::System, "Initial"));
    conv3 = conv1;
    assert(conv3.size() == conv1.size());
    assert(conv3.begin() != conv1.begin() && "Copy assignment must perform deep copy with distinct buffer addresses");
    assert(conv3.at(0).content() == "Msg 1");
    assert(conv3.at(1).content() == "Msg 2");

    // Modifying conv1 must not mutate conv2 or conv3
    conv1.append(Message(Role::User, "Msg 3"));
    assert(conv1.size() == 3);
    assert(conv2.size() == 2);
    assert(conv3.size() == 2);

    // Self-assignment safety
    Conversation *ptr2 = &conv2;
    conv2 = *ptr2;
    assert(conv2.size() == 2);
    assert(conv2.at(0).content() == "Msg 1");
    std::cout << "[PASS] Test 3: Rule of Five (Copy)\n";
}

// 4. Rule of Five (Move)
void test_rule_of_five_move()
{
    Conversation conv1;
    conv1.append(Message(Role::User, "Alpha"));
    conv1.append(Message(Role::Assistant, "Beta"));
    const Message *orig_ptr = conv1.begin();

    // Move constructor
    Conversation conv2 = std::move(conv1);
    assert(conv2.begin() == orig_ptr && "Move constructor must steal pointer");
    assert(conv2.size() == 2);
    assert(conv2.at(0).content() == "Alpha");
    assert(conv2.at(1).content() == "Beta");

    // Moved-from object must be valid, size 0, null buffer
    assert(conv1.size() == 0);
    assert(conv1.begin() == nullptr);
    assert(conv1.end() == nullptr);

    // Move assignment
    Conversation conv3;
    conv3.append(Message(Role::System, "Old data"));
    conv3 = std::move(conv2);
    assert(conv3.begin() == orig_ptr && "Move assignment must steal pointer");
    assert(conv3.size() == 2);
    assert(conv2.size() == 0);
    assert(conv2.begin() == nullptr);

    // Self move assignment safety
    Conversation *ptr3 = &conv3;
    conv3 = std::move(*ptr3);
    assert(conv3.size() == 2);
    assert(conv3.begin() == orig_ptr);
    std::cout << "[PASS] Test 4: Rule of Five (Move)\n";
}

// 5. Growth Behavior
void test_growth_behavior()
{
    Conversation conv;
    assert(conv.capacity() == 0);

    // Append 50 items and verify power-of-two capacity growth
    for (std::size_t i = 0; i < 50; ++i)
    {
        conv.append(Message(Role::User, "Message #" + std::to_string(i)));
        assert(conv.size() == i + 1);
        assert(conv.capacity() >= conv.size());
    }

    // Capacity must have doubled: 0 -> 1 -> 2 -> 4 -> 8 -> 16 -> 32 -> 64
    assert(conv.capacity() == 64);

    // Verify all 50 items retained exact integrity across all reallocations
    for (std::size_t i = 0; i < 50; ++i)
    {
        assert(conv.at(i).role() == Role::User);
        assert(conv.at(i).content() == "Message #" + std::to_string(i));
    }
    std::cout << "[PASS] Test 5: Growth Behavior\n";
}

// 6. Scanner Clean Text
void test_scanner_clean_text()
{
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    auto out1 = scanner.feed("Hello ");
    assert(!out1.sentinel_found);
    auto out2 = scanner.feed("world, this ");
    assert(!out2.sentinel_found);
    auto out3 = scanner.feed("is a clean stream.");
    assert(!out3.sentinel_found);
    auto out4 = scanner.flush();
    assert(!out4.sentinel_found);

    std::string full = out1.safe_text + out2.safe_text + out3.safe_text + out4.safe_text;
    assert(full == "Hello world, this is a clean stream.");
    std::cout << "[PASS] Test 6: Scanner Clean Text\n";
}

// 7. Scanner Split Sentinel
void test_scanner_split_sentinel()
{
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;

    for (std::size_t split = 0; split <= text.size(); ++split)
    {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "Sentinel must be caught regardless of split boundary");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
    std::cout << "[PASS] Test 7: Scanner Split Sentinel\n";
}

// 8. Scanner False Alarms
void test_scanner_false_alarms()
{
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    std::vector<std::string> tricky_chunks = {
        "Prefix text <|end_world|> is not the sentinel. ",
        "Also <|end_conversation without closing tag. ",
        "And partial <|end_ followed by different text.",
        "<|"};

    std::string assembled;
    for (const auto &chunk : tricky_chunks)
    {
        auto out = scanner.feed(chunk);
        assert(!out.sentinel_found && "False alarm: scanner must not trigger on near-matches");
        assembled += out.safe_text;
    }
    auto out_final = scanner.flush();
    assert(!out_final.sentinel_found);
    assembled += out_final.safe_text;

    std::string expected;
    for (const auto &c : tricky_chunks)
        expected += c;
    assert(assembled == expected && "All original text must be preserved across false alarm chunks");
    std::cout << "[PASS] Test 8: Scanner False Alarms\n";
}

// 9. Scanner Bounded Memory (4MB stream fed one byte at a time)
void test_scanner_bounded_memory()
{
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);
    const std::size_t max_pending = sentinel.size() - 1;

    // 4 Megabytes of adversarial repetitive input fed 1 byte at a time
    const std::string pattern = "<|end_conversa"; // 14 characters
    const std::size_t kCycles = (4 * 1024 * 1024) / pattern.size();
    const std::size_t kTotalBytes = kCycles * pattern.size();

    for (std::size_t i = 0; i < kTotalBytes; ++i)
    {
        char ch = pattern[i % pattern.size()];
        auto out = scanner.feed(std::string_view(&ch, 1));
        assert(!out.sentinel_found);
        assert(scanner.pending_size() <= max_pending &&
               "Invariant violation: pending_ buffer exceeded sentinel.size() - 1");
    }

    // Now feed the completing suffix
    auto out_complete = scanner.feed("tion|>");
    assert(out_complete.sentinel_found && "Sentinel should be caught after completing suffix");
    std::cout << "[PASS] Test 9: Scanner Bounded Memory (4MB 1-byte stream)\n";
}

// 10. Harness Turn Limit
void test_harness_turn_limit()
{
    const std::string script_path = "test_turn_limit.script";
    {
        std::ofstream script(script_path);
        script << "role: system\nTest harness.\n---\n";
        for (int i = 0; i < 10; ++i)
        {
            script << "role: assistant\nReply " << i << "\n---\n";
        }
    }

    HarnessConfig cfg;
    cfg.max_turns = 2;
    cfg.system_message = "Test harness.";

    auto model = std::make_unique<ScriptedModelClient>(script_path);
    Harness harness(std::move(model), cfg);

    MockInputSource in({"user turn 1", "user turn 2", "user turn 3"});
    MockOutputSink out;

    StopReason reason = harness.run(in, out);
    assert(reason.kind == StopReason::Kind::TurnLimit);
    // Pinned system + (user + assistant) * 2 = 5 messages
    assert(harness.conversation().size() == 5);
    assert(harness.conversation().at(0).role() == Role::System);
    assert(harness.conversation().at(1).role() == Role::User);
    assert(harness.conversation().at(2).role() == Role::Assistant);
    assert(harness.conversation().at(3).role() == Role::User);
    assert(harness.conversation().at(4).role() == Role::Assistant);

    std::remove(script_path.c_str());
    std::cout << "[PASS] Test 10: Harness Turn Limit\n";
}

// 11. Harness Sentinel Halt
void test_harness_sentinel_halt()
{
    const std::string script_path = "test_sentinel_halt.script";
    {
        std::ofstream script(script_path);
        script << "chunk: 3\nrole: assistant\nStop here now.<|end_conversation|>\n---\n";
        script << "role: assistant\nUnreachable block\n";
    }

    HarnessConfig cfg;
    cfg.max_turns = 5;

    auto model = std::make_unique<ScriptedModelClient>(script_path);
    Harness harness(std::move(model), cfg);

    MockInputSource in({"hello"});
    MockOutputSink out;

    StopReason reason = harness.run(in, out);
    assert(reason.kind == StopReason::Kind::Sentinel);
    // Conversation should have 2 messages: user and assistant
    assert(harness.conversation().size() == 2);
    // Terminal output must NOT contain the sentinel string
    assert(out.output().find("<|end_conversation|>") == std::string::npos);
    assert(out.output().find("Stop here now.") != std::string::npos);
    // The stored message IN conversation must contain the sentinel for transcript replaying
    assert(harness.conversation().at(1).content().find("<|end_conversation|>") != std::string::npos);

    std::remove(script_path.c_str());
    std::cout << "[PASS] Test 11: Harness Sentinel Halt\n";
}

// 12. Transcript Round-Trip
void test_transcript_round_trip()
{
    const std::string recorded_transcript = "recorded_transcript.txt";
    const std::string replayed_transcript = "replayed_transcript.txt";

    {
        std::ofstream f(recorded_transcript);
        f << "role: system\nBe ultra concise.\n---\n";
        f << "role: user\nHowdy!\n---\n";
        f << "role: assistant\nGreetings! What can I do for you?\n---\n";
        f << "role: user\nSee you later!\n---\n";
        f << "role: assistant\nGoodbye.<|end_conversation|>\n";
    }

    auto replay_model = std::make_unique<ReplayModelClient>(recorded_transcript);
    HarnessConfig cfg;
    cfg.max_turns = 10;
    cfg.system_message = replay_model->system_message();

    Harness harness(std::move(replay_model), cfg);
    MockInputSource in({"Howdy!", "See you later!"});
    MockOutputSink out;

    StopReason reason = harness.run(in, out);
    assert(reason.kind == StopReason::Kind::Sentinel);

    save_transcript(harness.conversation(), replayed_transcript);

    // Read both files and compare line-by-line
    std::ifstream orig_file(recorded_transcript);
    std::ifstream replay_file(replayed_transcript);
    assert(orig_file.is_open() && replay_file.is_open());

    std::string orig_line, replay_line;
    while (std::getline(orig_file, orig_line))
    {
        assert(std::getline(replay_file, replay_line));
        assert(orig_line == replay_line);
    }
    assert(!std::getline(replay_file, replay_line)); // replay must have exact same number of lines

    std::remove(recorded_transcript.c_str());
    std::remove(replayed_transcript.c_str());
    std::cout << "[PASS] Test 12: Transcript Round-Trip\n";
}

int main()
{
    std::cout << "Running ECE 309 Project 2 Test Suite...\n\n";

    test_empty_conversation_bounds();
    test_system_message_ordering();
    test_rule_of_five_copy();
    test_rule_of_five_move();
    test_growth_behavior();
    test_scanner_clean_text();
    test_scanner_split_sentinel();
    test_scanner_false_alarms();
    test_scanner_bounded_memory();
    test_harness_turn_limit();
    test_harness_sentinel_halt();
    test_transcript_round_trip();

    std::cout << "\nAll 12 required test cases PASSED successfully under AddressSanitizer!\n";
    return 0;
}
