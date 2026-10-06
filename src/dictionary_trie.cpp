#include <broime/ime.h>

#include <brosearch/fuzzy.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <queue>
#include <span>
#include <string>
#include <vector>

namespace bro::ime {

namespace {

constexpr uint32_t MAGIC_BIME = 0x42494D45; // "BIME"
constexpr uint32_t FORMAT_VERSION = 1;

struct TrieNode {
    uint8_t byte_val = 0;
    bool is_word = false;
    int32_t score = 0;
    std::string word;
    std::string annotation;
    std::vector<std::unique_ptr<TrieNode>> children;

    TrieNode* find_child(uint8_t b) const {
        for (const auto& child : children) {
            if (child->byte_val == b) {
                return child.get();
            }
        }
        return nullptr;
    }

    TrieNode* get_or_add_child(uint8_t b) {
        for (const auto& child : children) {
            if (child->byte_val == b) {
                return child.get();
            }
        }
        auto node = std::make_unique<TrieNode>();
        node->byte_val = b;
        TrieNode* ptr = node.get();
        children.push_back(std::move(node));
        return ptr;
    }
};

void collect_candidates(const TrieNode* node, std::vector<CandidateEntry>& results) {
    if (!node) return;
    if (node->is_word) {
        CandidateEntry entry;
        entry.text = node->word;
        entry.annotation = node->annotation;
        entry.score = node->score;
        results.push_back(std::move(entry));
    }
    for (const auto& child : node->children) {
        collect_candidates(child.get(), results);
    }
}

void collect_all_words(const TrieNode* node, std::vector<std::string>& words, std::vector<CandidateEntry>& entries) {
    if (!node) return;
    if (node->is_word) {
        words.push_back(node->word);
        CandidateEntry entry;
        entry.text = node->word;
        entry.annotation = node->annotation;
        entry.score = node->score;
        entries.push_back(std::move(entry));
    }
    for (const auto& child : node->children) {
        collect_all_words(child.get(), words, entries);
    }
}

void serialize_node(const TrieNode* node, std::vector<uint8_t>& buf) {
    buf.push_back(node->byte_val);
    uint8_t flags = node->is_word ? 1 : 0;
    buf.push_back(flags);

    auto child_count = static_cast<uint16_t>(node->children.size());
    buf.push_back(static_cast<uint8_t>(child_count & 0xFF));
    buf.push_back(static_cast<uint8_t>((child_count >> 8) & 0xFF));

    if (node->is_word) {
        // 4 bytes score
        uint32_t s = 0;
        std::memcpy(&s, &node->score, sizeof(s));
        for (int i = 0; i < 4; ++i) {
            buf.push_back(static_cast<uint8_t>((s >> (i * 8)) & 0xFF));
        }

        // word string
        auto wlen = static_cast<uint16_t>(node->word.size());
        buf.push_back(static_cast<uint8_t>(wlen & 0xFF));
        buf.push_back(static_cast<uint8_t>((wlen >> 8) & 0xFF));
        buf.insert(buf.end(), node->word.begin(), node->word.end());

        // annotation string
        auto alen = static_cast<uint16_t>(node->annotation.size());
        buf.push_back(static_cast<uint8_t>(alen & 0xFF));
        buf.push_back(static_cast<uint8_t>((alen >> 8) & 0xFF));
        buf.insert(buf.end(), node->annotation.begin(), node->annotation.end());
    }

    for (const auto& child : node->children) {
        serialize_node(child.get(), buf);
    }
}

std::unique_ptr<TrieNode> deserialize_node(std::span<const uint8_t> bytes, size_t& offset, size_t& words_read, size_t& nodes_read) {
    if (offset + 4 > bytes.size()) {
        return nullptr;
    }

    auto node = std::make_unique<TrieNode>();
    node->byte_val = bytes[offset++];
    uint8_t flags = bytes[offset++];
    node->is_word = (flags & 1) != 0;

    uint16_t child_count = static_cast<uint16_t>(bytes[offset]) |
                          (static_cast<uint16_t>(bytes[offset + 1]) << 8);
    offset += 2;
    ++nodes_read;

    if (node->is_word) {
        if (offset + 4 > bytes.size()) return nullptr;
        uint32_t s = static_cast<uint32_t>(bytes[offset]) |
                     (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
                     (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
                     (static_cast<uint32_t>(bytes[offset + 3]) << 24);
        offset += 4;
        std::memcpy(&node->score, &s, sizeof(s));

        if (offset + 2 > bytes.size()) return nullptr;
        uint16_t wlen = static_cast<uint16_t>(bytes[offset]) |
                        (static_cast<uint16_t>(bytes[offset + 1]) << 8);
        offset += 2;

        if (offset + wlen > bytes.size()) return nullptr;
        node->word.assign(reinterpret_cast<const char*>(&bytes[offset]), wlen);
        offset += wlen;

        if (offset + 2 > bytes.size()) return nullptr;
        uint16_t alen = static_cast<uint16_t>(bytes[offset]) |
                        (static_cast<uint16_t>(bytes[offset + 1]) << 8);
        offset += 2;

        if (offset + alen > bytes.size()) return nullptr;
        node->annotation.assign(reinterpret_cast<const char*>(&bytes[offset]), alen);
        offset += alen;

        ++words_read;
    }

    node->children.reserve(child_count);
    for (uint16_t i = 0; i < child_count; ++i) {
        auto child = deserialize_node(bytes, offset, words_read, nodes_read);
        if (!child) return nullptr;
        node->children.push_back(std::move(child));
    }

    return node;
}

} // namespace

struct DictionaryTrie::Impl {
    std::unique_ptr<TrieNode> root = std::make_unique<TrieNode>();
    size_t word_count = 0;
    size_t node_count = 1;

    void clear() {
        root = std::make_unique<TrieNode>();
        word_count = 0;
        node_count = 1;
    }
};

DictionaryTrie::DictionaryTrie() : impl_(std::make_unique<Impl>()) {}
DictionaryTrie::~DictionaryTrie() = default;
DictionaryTrie::DictionaryTrie(DictionaryTrie&&) noexcept = default;
DictionaryTrie& DictionaryTrie::operator=(DictionaryTrie&&) noexcept = default;

void DictionaryTrie::insert(std::string_view word, int32_t score, std::string_view annotation) {
    if (word.empty()) return;

    TrieNode* current = impl_->root.get();
    for (char c : word) {
        auto b = static_cast<uint8_t>(c);
        TrieNode* next = current->find_child(b);
        if (!next) {
            next = current->get_or_add_child(b);
            ++impl_->node_count;
        }
        current = next;
    }

    if (!current->is_word) {
        ++impl_->word_count;
    }
    current->is_word = true;
    current->score = score;
    current->word = std::string(word);
    current->annotation = std::string(annotation);
}

bool DictionaryTrie::contains(std::string_view word) const {
    if (word.empty()) return false;
    const TrieNode* current = impl_->root.get();
    for (char c : word) {
        current = current->find_child(static_cast<uint8_t>(c));
        if (!current) return false;
    }
    return current->is_word;
}

std::vector<CandidateEntry> DictionaryTrie::lookup_exact(std::string_view word) const {
    std::vector<CandidateEntry> results;
    if (word.empty()) return results;

    const TrieNode* current = impl_->root.get();
    for (char c : word) {
        current = current->find_child(static_cast<uint8_t>(c));
        if (!current) return results;
    }

    if (current->is_word) {
        CandidateEntry entry;
        entry.text = current->word;
        entry.annotation = current->annotation;
        entry.score = current->score;
        entry.match_query = std::string(word);
        results.push_back(std::move(entry));
    }
    return results;
}

std::vector<CandidateEntry> DictionaryTrie::lookup_prefix(std::string_view prefix, size_t max_results) const {
    std::vector<CandidateEntry> results;
    if (max_results == 0) return results;

    const TrieNode* current = impl_->root.get();
    for (char c : prefix) {
        current = current->find_child(static_cast<uint8_t>(c));
        if (!current) return results;
    }

    collect_candidates(current, results);
    for (auto& entry : results) {
        entry.match_query = std::string(prefix);
    }

    // Sort by score (descending), then by length (ascending), then alphabetically
    std::sort(results.begin(), results.end(), [](const CandidateEntry& a, const CandidateEntry& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        if (a.text.size() != b.text.size()) {
            return a.text.size() < b.text.size();
        }
        return a.text < b.text;
    });

    if (results.size() > max_results) {
        results.resize(max_results);
    }

    return results;
}

std::vector<CandidateEntry> DictionaryTrie::lookup_fuzzy(std::string_view query, size_t max_results) const {
    std::vector<CandidateEntry> results;
    if (query.empty() || max_results == 0) return results;

    std::vector<std::string> words;
    std::vector<CandidateEntry> all_entries;
    collect_all_words(impl_->root.get(), words, all_entries);

    if (words.empty()) return results;

    bro::search::FuzzyOptions opts;
    opts.case_mode = bro::search::FuzzyCase::Smart;
    bro::search::FuzzyQuery fq(query, opts);

    auto fuzzy_matches = bro::search::fuzzy_filter(fq, std::span<const std::string>(words), max_results);
    results.reserve(fuzzy_matches.size());

    for (const auto& match : fuzzy_matches) {
        if (match.index < all_entries.size()) {
            CandidateEntry entry = all_entries[match.index];
            entry.match_query = std::string(query);
            entry.score = match.score;
            results.push_back(std::move(entry));
        }
    }

    return results;
}

size_t DictionaryTrie::word_count() const noexcept {
    return impl_->word_count;
}

size_t DictionaryTrie::node_count() const noexcept {
    return impl_->node_count;
}

void DictionaryTrie::clear() noexcept {
    impl_->clear();
}

std::vector<uint8_t> DictionaryTrie::serialize() const {
    std::vector<uint8_t> buf;
    buf.reserve(128 + impl_->word_count * 16);

    // Write header: MAGIC, VERSION, word_count, node_count
    auto append_u32 = [&buf](uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            buf.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
        }
    };

    append_u32(MAGIC_BIME);
    append_u32(FORMAT_VERSION);
    append_u32(static_cast<uint32_t>(impl_->word_count));
    append_u32(static_cast<uint32_t>(impl_->node_count));

    serialize_node(impl_->root.get(), buf);
    return buf;
}

bool DictionaryTrie::deserialize(std::span<const uint8_t> bytes) {
    if (bytes.size() < 16) return false;

    auto read_u32 = [&bytes](size_t off) -> uint32_t {
        return static_cast<uint32_t>(bytes[off]) |
               (static_cast<uint32_t>(bytes[off + 1]) << 8) |
               (static_cast<uint32_t>(bytes[off + 2]) << 16) |
               (static_cast<uint32_t>(bytes[off + 3]) << 24);
    };

    uint32_t magic = read_u32(0);
    uint32_t version = read_u32(4);
    uint32_t expected_words = read_u32(8);
    uint32_t expected_nodes = read_u32(12);

    if (magic != MAGIC_BIME || version != FORMAT_VERSION) {
        return false;
    }

    size_t offset = 16;
    size_t words_read = 0;
    size_t nodes_read = 0;

    auto new_root = deserialize_node(bytes, offset, words_read, nodes_read);
    if (!new_root || words_read != expected_words || nodes_read != expected_nodes) {
        return false;
    }

    impl_->root = std::move(new_root);
    impl_->word_count = words_read;
    impl_->node_count = nodes_read;
    return true;
}

bool DictionaryTrie::save_to_file(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    auto bytes = serialize();
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return file.good();
}

bool DictionaryTrie::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    auto size = file.tellg();
    if (size <= 0) return false;
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    if (!file.good()) return false;
    return deserialize(buffer);
}

} // namespace bro::ime
