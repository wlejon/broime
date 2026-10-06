#include <broime/ime.h>
#include "check.h"

using namespace bro::ime;

int main() {
    ImeEngine engine;

    // 1. Compose workflow inside ImeEngine
    {
        std::string committed;
        bool consumed = engine.feed_key(keysyms::DeadAcute, &committed);
        CHECK(consumed);
        CHECK(committed.empty());
        CHECK(engine.is_active());

        consumed = engine.feed_key('e', &committed);
        CHECK(consumed);
        CHECK_EQ(committed, "é");
        CHECK(!engine.is_active());
        CHECK(engine.preedit_text().empty());

        // MultiKey compose
        engine.feed_key(keysyms::MultiKey, &committed);
        engine.feed_key('c', &committed);
        engine.feed_key('o', &committed);
        CHECK_EQ(committed, "©");
    }

    // 2. Dictionary candidate workflow
    {
        engine.dictionary().insert("nihao", 100, "hello");
        engine.dictionary().insert("nimen", 80, "you all");
        engine.dictionary().insert("ni", 90, "you");

        std::string committed;
        // Type 'n', 'i'
        engine.feed_key('n', &committed);
        engine.feed_key('i', &committed);

        CHECK_EQ(engine.preedit_text(), "ni");
        CHECK(engine.is_active());
        CHECK(engine.candidates().has_candidates());
        CHECK_EQ(engine.candidates().candidate_count(), 3u);

        // Candidates: "ni" (score 90), "nihao" (100), "nimen" (80)
        // With descending score: nihao (100), ni (90), nimen (80)
        CHECK_EQ(engine.candidates().selected_candidate()->text, "nihao");

        // Space commits currently selected candidate
        engine.feed_key(keysyms::Space, &committed);
        CHECK_EQ(committed, "nihao");
        CHECK(!engine.is_active());
        CHECK(engine.preedit_text().empty());

        // Type 'n', 'i' again and select with number key '2'
        engine.feed_key('n', &committed);
        engine.feed_key('i', &committed);
        // Candidate 1: nihao, Candidate 2: ni
        engine.feed_key('2', &committed);
        CHECK_EQ(committed, "ni");
        CHECK(!engine.is_active());

        // 3. Backspace in preedit
        engine.feed_key('n', &committed);
        engine.feed_key('i', &committed);
        CHECK_EQ(engine.preedit_text(), "ni");
        engine.feed_key(keysyms::BackSpace, &committed);
        CHECK_EQ(engine.preedit_text(), "n");
        engine.feed_key(keysyms::BackSpace, &committed);
        CHECK_EQ(engine.preedit_text(), "");
        CHECK(!engine.candidates().has_candidates());

        // 4. Escape clears preedit
        engine.feed_key('n', &committed);
        engine.feed_key('i', &committed);
        CHECK(engine.is_active());
        engine.feed_key(keysyms::Escape, &committed);
        CHECK(!engine.is_active());
        CHECK(engine.preedit_text().empty());

        // 5. Return commits raw preedit
        engine.feed_key('n', &committed);
        engine.feed_key('i', &committed);
        engine.feed_key(keysyms::Return, &committed);
        CHECK_EQ(committed, "ni");
        CHECK(!engine.is_active());
    }

    // 3. Bypass mode
    {
        engine.set_mode(InputMode::Bypass);
        std::string committed;
        bool consumed = engine.feed_key('a', &committed);
        CHECK(!consumed);
        CHECK(committed.empty());
    }

    return bstest::finish("test_ime_engine");
}
