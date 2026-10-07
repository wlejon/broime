#include <broime/ime.h>
#include "check.h"

#include <filesystem>
#include <fstream>

using namespace bro::ime;

int main() {
    ComposeEngine engine;
    engine.clear_rules();
    CHECK_EQ(engine.rule_count(), 0u);

    // 1. Basic string parsing with comments and spaces
    std::string_view xcompose_sample = R"(
# XCompose test configuration
include "%L"

<Multi_key> <minus> <greater> : "→" # Right arrow
<Multi_key> <less> <minus> : "←"    # Left arrow
<Multi_key> <t> <e> <s> <t> : "passed"
<dead_acute> <e> : "é"
<Multi_key> <q> <u> <o> <t> <e> : "hello \"world\""
<Multi_key> <n> <l> : "line1\nline2"
<Multi_key> <h> <e> <x> : "\x41\x42"
)";

    bool ok = engine.load_xcompose_string(xcompose_sample);
    CHECK(ok);
    CHECK_GE(engine.rule_count(), 6u);

    // Test parsed rules
    engine.feed(keysyms::MultiKey);
    engine.feed('-');
    auto r1 = engine.feed('>');
    CHECK_EQ(r1.status, ComposeResult::Matched);
    CHECK_EQ(r1.text, "→");

    // Test quote escaping
    engine.feed(keysyms::MultiKey);
    engine.feed('q');
    engine.feed('u');
    engine.feed('o');
    engine.feed('t');
    auto r_quote = engine.feed('e');
    CHECK_EQ(r_quote.status, ComposeResult::Matched);
    CHECK_EQ(r_quote.text, "hello \"world\"");

    // Test newline escape
    engine.feed(keysyms::MultiKey);
    engine.feed('n');
    auto r_nl = engine.feed('l');
    CHECK_EQ(r_nl.status, ComposeResult::Matched);
    CHECK_EQ(r_nl.text, "line1\nline2");

    // Test hex escape (\x41\x42 -> "AB")
    engine.feed(keysyms::MultiKey);
    engine.feed('h');
    engine.feed('e');
    auto r_hex = engine.feed('x');
    CHECK_EQ(r_hex.status, ComposeResult::Matched);
    CHECK_EQ(r_hex.text, "AB");

    // 2. Load from file
    std::string temp_file =
        (std::filesystem::temp_directory_path() / "test_xcompose_file.txt").string();
    {
        std::ofstream out(temp_file);
        out << "<Multi_key> <f> <i> <l> <e> : \"file_match\"\n";
    }
    CHECK(engine.load_xcompose_file(temp_file));
    std::filesystem::remove(temp_file);

    engine.feed(keysyms::MultiKey);
    engine.feed('f');
    engine.feed('i');
    engine.feed('l');
    auto r_file = engine.feed('e');
    CHECK_EQ(r_file.status, ComposeResult::Matched);
    CHECK_EQ(r_file.text, "file_match");

    return bstest::finish("test_xcompose_parser");
}
