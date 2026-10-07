#include <broime/ime.h>
#include "check.h"

#include <filesystem>

using namespace bro::ime;

int main() {
    DictionaryTrie trie;

    // 1. Basic insertion and lookup
    trie.insert("hello", 100, "greeting");
    trie.insert("help", 80, "support");
    trie.insert("helmet", 60, "gear");
    trie.insert("hell", 90, "place");
    trie.insert("hero", 50, "character");
    trie.insert("world", 100, "noun");

    CHECK_EQ(trie.word_count(), 6u);
    CHECK(trie.contains("hello"));
    CHECK(trie.contains("help"));
    CHECK(trie.contains("world"));
    CHECK(!trie.contains("hel")); // prefix only, not a word
    CHECK(!trie.contains("heroic"));

    // 2. Exact lookup
    auto exact_res = trie.lookup_exact("hello");
    REQUIRE_EQ(exact_res.size(), 1u);
    CHECK_EQ(exact_res[0].text, "hello");
    CHECK_EQ(exact_res[0].annotation, "greeting");
    CHECK_EQ(exact_res[0].score, 100);

    CHECK(trie.lookup_exact("nonexistent").empty());

    // 3. Prefix lookup & scoring rank
    auto prefix_res = trie.lookup_prefix("hel", 10);
    REQUIRE_EQ(prefix_res.size(), 4u); // hello (100), hell (90), help (80), helmet (60)
    CHECK_EQ(prefix_res[0].text, "hello");
    CHECK_EQ(prefix_res[1].text, "hell");
    CHECK_EQ(prefix_res[2].text, "help");
    CHECK_EQ(prefix_res[3].text, "helmet");

    // Max results limit
    auto limit_res = trie.lookup_prefix("hel", 2);
    CHECK_EQ(limit_res.size(), 2u);
    CHECK_EQ(limit_res[0].text, "hello");
    CHECK_EQ(limit_res[1].text, "hell");

    // Nonexistent prefix
    CHECK(trie.lookup_prefix("xyz", 10).empty());

    // 4. Binary serialization round-trip
    std::vector<uint8_t> binary_blob = trie.serialize();
    CHECK_GE(binary_blob.size(), 16u);

    DictionaryTrie deserialized_trie;
    bool des_ok = deserialized_trie.deserialize(binary_blob);
    CHECK(des_ok);
    CHECK_EQ(deserialized_trie.word_count(), trie.word_count());
    CHECK_EQ(deserialized_trie.node_count(), trie.node_count());

    CHECK(deserialized_trie.contains("hello"));
    CHECK(deserialized_trie.contains("helmet"));
    CHECK(!deserialized_trie.contains("hel"));

    auto des_prefix = deserialized_trie.lookup_prefix("hel", 10);
    REQUIRE_EQ(des_prefix.size(), 4u);
    CHECK_EQ(des_prefix[0].text, "hello");
    CHECK_EQ(des_prefix[3].text, "helmet");

    // 5. File I/O save & load
    std::string temp_path =
        (std::filesystem::temp_directory_path() / "test_broime_dict.bin").string();
    CHECK(trie.save_to_file(temp_path));

    DictionaryTrie file_trie;
    CHECK(file_trie.load_from_file(temp_path));
    std::filesystem::remove(temp_path);

    CHECK_EQ(file_trie.word_count(), 6u);
    CHECK(file_trie.contains("world"));

    // 6. Fuzzy lookup using brosearch
    auto fuzzy_res = trie.lookup_fuzzy("helo", 5);
    REQUIRE(!fuzzy_res.empty());
    CHECK_EQ(fuzzy_res[0].text, "hello");

    return bstest::finish("test_dictionary_trie");
}
