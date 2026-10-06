#include <broime/ime.h>
#include "check.h"

using namespace bro::ime;

int main() {
    ComposeEngine engine;

    // 1. Dead key tests
    {
        // Dead acute + e -> é
        auto r1 = engine.feed(keysyms::DeadAcute);
        CHECK_EQ(r1.status, ComposeResult::Pending);
        CHECK(engine.is_composing());

        auto r2 = engine.feed('e');
        CHECK_EQ(r2.status, ComposeResult::Matched);
        CHECK_EQ(r2.text, "é");
        CHECK(!engine.is_composing());

        // Dead tilde + a -> ã
        engine.feed(keysyms::DeadTilde);
        auto r_tilde = engine.feed('a');
        CHECK_EQ(r_tilde.status, ComposeResult::Matched);
        CHECK_EQ(r_tilde.text, "ã");

        // Dead grave + a -> à
        engine.feed(keysyms::DeadGrave);
        auto r_grave = engine.feed('a');
        CHECK_EQ(r_grave.status, ComposeResult::Matched);
        CHECK_EQ(r_grave.text, "à");

        // Dead circumflex + o -> ô
        engine.feed(keysyms::DeadCircumflex);
        auto r_circ = engine.feed('o');
        CHECK_EQ(r_circ.status, ComposeResult::Matched);
        CHECK_EQ(r_circ.text, "ô");

        // Dead diaeresis + u -> ü
        engine.feed(keysyms::DeadDiaeresis);
        auto r_dia = engine.feed('u');
        CHECK_EQ(r_dia.status, ComposeResult::Matched);
        CHECK_EQ(r_dia.text, "ü");

        // Dead cedilla + c -> ç
        engine.feed(keysyms::DeadCedilla);
        auto r_ced = engine.feed('c');
        CHECK_EQ(r_ced.status, ComposeResult::Matched);
        CHECK_EQ(r_ced.text, "ç");

        // Dead caron + c -> č
        engine.feed(keysyms::DeadCaron);
        auto r_car = engine.feed('c');
        CHECK_EQ(r_car.status, ComposeResult::Matched);
        CHECK_EQ(r_car.text, "č");

        // Dead abovering + a -> å
        engine.feed(keysyms::DeadAboveRing);
        auto r_ring = engine.feed('a');
        CHECK_EQ(r_ring.status, ComposeResult::Matched);
        CHECK_EQ(r_ring.text, "å");
    }

    // 2. Multi_key (Compose) sequences
    {
        // Multi_key + ' + e -> é
        CHECK_EQ(engine.feed(keysyms::MultiKey).status, ComposeResult::Pending);
        CHECK_EQ(engine.feed('\'').status, ComposeResult::Pending);
        auto r_match = engine.feed('e');
        CHECK_EQ(r_match.status, ComposeResult::Matched);
        CHECK_EQ(r_match.text, "é");

        // Multi_key + c + o -> ©
        engine.feed(keysyms::MultiKey);
        engine.feed('c');
        auto r_copy = engine.feed('o');
        CHECK_EQ(r_copy.status, ComposeResult::Matched);
        CHECK_EQ(r_copy.text, "©");

        // Multi_key + - + > -> →
        engine.feed(keysyms::MultiKey);
        engine.feed('-');
        auto r_arr = engine.feed('>');
        CHECK_EQ(r_arr.status, ComposeResult::Matched);
        CHECK_EQ(r_arr.text, "→");

        // Multi_key + = + > -> ⇒
        engine.feed(keysyms::MultiKey);
        engine.feed('=');
        auto r_darr = engine.feed('>');
        CHECK_EQ(r_darr.status, ComposeResult::Matched);
        CHECK_EQ(r_darr.text, "⇒");

        // Multi_key + s + s -> ß
        engine.feed(keysyms::MultiKey);
        engine.feed('s');
        auto r_ss = engine.feed('s');
        CHECK_EQ(r_ss.status, ComposeResult::Matched);
        CHECK_EQ(r_ss.text, "ß");

        // Multi_key + t + m -> ™
        engine.feed(keysyms::MultiKey);
        engine.feed('t');
        auto r_tm = engine.feed('m');
        CHECK_EQ(r_tm.status, ComposeResult::Matched);
        CHECK_EQ(r_tm.text, "™");

        // Multi_key + 1 + 2 -> ½
        engine.feed(keysyms::MultiKey);
        engine.feed('1');
        auto r_half = engine.feed('2');
        CHECK_EQ(r_half.status, ComposeResult::Matched);
        CHECK_EQ(r_half.text, "½");
    }

    // 3. ASCII accent combinations
    {
        // ' + e -> é
        engine.feed('\'');
        auto r_ae = engine.feed('e');
        CHECK_EQ(r_ae.status, ComposeResult::Matched);
        CHECK_EQ(r_ae.text, "é");

        // ~ + a -> ã
        engine.feed('~');
        auto r_ta = engine.feed('a');
        CHECK_EQ(r_ta.status, ComposeResult::Matched);
        CHECK_EQ(r_ta.text, "ã");

        // ' + ' -> '
        engine.feed('\'');
        auto r_sq = engine.feed('\'');
        CHECK_EQ(r_sq.status, ComposeResult::Matched);
        CHECK_EQ(r_sq.text, "'");

        // ' + space -> '
        engine.feed('\'');
        auto r_sp = engine.feed(keysyms::Space);
        CHECK_EQ(r_sp.status, ComposeResult::Matched);
        CHECK_EQ(r_sp.text, "'");
    }

    // 4. Invalid sequence handling & reset
    {
        // Dead acute + 'z' -> 'ź' (exists)
        // Dead acute + '9' -> not an accented letter, should cancel and return None
        engine.feed(keysyms::DeadAcute);
        auto r_inv = engine.feed('9');
        CHECK_EQ(r_inv.status, ComposeResult::None);
        CHECK(!engine.is_composing());

        // Unrelated non-compose key returns None immediately
        auto r_norm = engine.feed('x');
        CHECK_EQ(r_norm.status, ComposeResult::None);
        CHECK(!engine.is_composing());
    }

    // 5. Custom rule addition
    {
        size_t initial_count = engine.rule_count();
        engine.add_rule("<Multi_key> <f> <o> <o>", "🎉");
        CHECK_GT(engine.rule_count(), initial_count);

        engine.feed(keysyms::MultiKey);
        engine.feed('f');
        engine.feed('o');
        auto r_custom = engine.feed('o');
        CHECK_EQ(r_custom.status, ComposeResult::Matched);
        CHECK_EQ(r_custom.text, "🎉");
    }

    return bstest::finish("test_compose");
}
