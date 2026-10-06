#include <broime/types.h>
#include "check.h"

using namespace bro::ime;

int main() {
    // 1. Keysym from name & to name
    CHECK_EQ(keysym_from_name("Multi_key"), keysyms::MultiKey);
    CHECK_EQ(keysym_from_name("dead_acute"), keysyms::DeadAcute);
    CHECK_EQ(keysym_from_name("dead_tilde"), keysyms::DeadTilde);
    CHECK_EQ(keysym_from_name("dead_grave"), keysyms::DeadGrave);
    CHECK_EQ(keysym_from_name("dead_circumflex"), keysyms::DeadCircumflex);
    CHECK_EQ(keysym_from_name("dead_diaeresis"), keysyms::DeadDiaeresis);
    CHECK_EQ(keysym_from_name("apostrophe"), static_cast<KeySym>('\''));
    CHECK_EQ(keysym_from_name("space"), keysyms::Space);
    CHECK_EQ(keysym_from_name("a"), static_cast<KeySym>('a'));
    CHECK_EQ(keysym_from_name("Z"), static_cast<KeySym>('Z'));
    CHECK_EQ(keysym_from_name("0xFE51"), keysyms::DeadAcute);

    CHECK_EQ(keysym_to_name(keysyms::MultiKey), "Multi_key");
    CHECK_EQ(keysym_to_name(keysyms::DeadAcute), "dead_acute");
    CHECK_EQ(keysym_to_name('e'), "e");

    // 2. Keysym to UTF-8
    CHECK_EQ(keysym_to_utf8('a'), "a");
    CHECK_EQ(keysym_to_utf8(0x00E9), "é");
    CHECK_EQ(keysym_from_utf8("a"), static_cast<KeySym>('a'));
    CHECK_EQ(keysym_from_utf8("é"), static_cast<KeySym>(0x00E9));

    // 3. ComposeSequence
    ComposeSequence seq{keysyms::MultiKey, '\'', 'e'};
    CHECK_EQ(seq.size(), 3u);
    CHECK(!seq.empty());
    CHECK_EQ(seq[0], keysyms::MultiKey);
    CHECK_EQ(seq[1], static_cast<KeySym>('\''));
    CHECK_EQ(seq[2], static_cast<KeySym>('e'));

    std::string str = seq.to_string();
    CHECK_EQ(str, "<Multi_key> <apostrophe> <e>");

    ComposeSequence parsed = ComposeSequence::from_string("<Multi_key> <apostrophe> <e>");
    CHECK_EQ(parsed.size(), 3u);
    CHECK_EQ(parsed, seq);

    // 4. ComposeResult
    ComposeResult r_none(ComposeResult::None);
    CHECK(r_none.is_none());
    CHECK(!r_none.is_pending());
    CHECK(!r_none.is_matched());
    CHECK(r_none == ComposeResult::None);

    ComposeResult r_pend(ComposeResult::Pending);
    CHECK(r_pend.is_pending());

    ComposeResult r_match(ComposeResult::Matched, "é");
    CHECK(r_match.is_matched());
    CHECK_EQ(r_match.text, "é");
    CHECK(r_match == ComposeResult::Matched);

    // 5. AnchorRect
    AnchorRect r{10, 20, 100, 50};
    CHECK_EQ(r.left(), 10);
    CHECK_EQ(r.top(), 20);
    CHECK_EQ(r.right(), 110);
    CHECK_EQ(r.bottom(), 70);
    CHECK(!r.is_empty());
    CHECK(r.contains(50, 40));
    CHECK(!r.contains(5, 40));
    CHECK(!r.contains(50, 80));

    AnchorRect r2{60, 40, 100, 100};
    CHECK(r.intersects(r2));
    AnchorRect r3{200, 200, 10, 10};
    CHECK(!r.intersects(r3));

    return bstest::finish("test_types");
}
