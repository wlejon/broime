#pragma once

#include <broime/types.h>

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bro::ime {

class ComposeEngine {
public:
    ComposeEngine();
    ~ComposeEngine();

    ComposeEngine(const ComposeEngine&) = delete;
    ComposeEngine& operator=(const ComposeEngine&) = delete;
    ComposeEngine(ComposeEngine&&) noexcept;
    ComposeEngine& operator=(ComposeEngine&&) noexcept;

    void load_builtin_rules();
    bool load_xcompose_string(std::string_view content);
    bool load_xcompose_file(const std::string& path);

    void add_rule(const ComposeSequence& seq, std::string_view utf8_output);
    void add_rule(std::string_view sequence_spec, std::string_view utf8_output);
    void clear_rules() noexcept;

    ComposeResult feed(KeySym sym);
    void reset() noexcept;

    [[nodiscard]] bool is_composing() const noexcept;
    [[nodiscard]] const ComposeSequence& current_sequence() const noexcept;
    [[nodiscard]] std::string_view matched_text() const noexcept;
    [[nodiscard]] size_t rule_count() const noexcept;

    [[nodiscard]] bool allow_ascii_accents() const noexcept;
    void set_allow_ascii_accents(bool enable) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class CandidateManager {
public:
    CandidateManager();
    ~CandidateManager();

    CandidateManager(const CandidateManager&) = default;
    CandidateManager& operator=(const CandidateManager&) = default;
    CandidateManager(CandidateManager&&) noexcept = default;
    CandidateManager& operator=(CandidateManager&&) noexcept = default;

    void set_candidates(std::vector<CandidateEntry> entries);
    void add_candidate(CandidateEntry entry);
    void clear() noexcept;

    [[nodiscard]] size_t candidate_count() const noexcept;
    [[nodiscard]] bool has_candidates() const noexcept;

    [[nodiscard]] size_t page_size() const noexcept;
    void set_page_size(size_t size) noexcept;

    [[nodiscard]] size_t total_pages() const noexcept;
    [[nodiscard]] size_t current_page_index() const noexcept;
    [[nodiscard]] size_t selected_index() const noexcept;

    bool select_next();
    bool select_prev();
    bool next_page();
    bool prev_page();
    bool select_index(size_t global_index);
    bool select_page_index(size_t index_in_page);

    [[nodiscard]] CandidatePage current_page() const;
    [[nodiscard]] const CandidateEntry* selected_candidate() const noexcept;
    [[nodiscard]] const std::vector<CandidateEntry>& all_candidates() const noexcept;

    void filter_fuzzy(std::string_view query);

    static PopupPlacement compute_placement(
        const AnchorRect& cursor_rect,
        int popup_width,
        int popup_height,
        const AnchorRect& display_bounds,
        int margin = 4
    );

private:
    std::vector<CandidateEntry> candidates_;
    std::vector<CandidateEntry> original_candidates_;
    size_t selected_index_ = 0;
    size_t page_size_ = 9;
};

class DictionaryTrie {
public:
    DictionaryTrie();
    ~DictionaryTrie();

    DictionaryTrie(const DictionaryTrie&) = delete;
    DictionaryTrie& operator=(const DictionaryTrie&) = delete;
    DictionaryTrie(DictionaryTrie&&) noexcept;
    DictionaryTrie& operator=(DictionaryTrie&&) noexcept;

    void insert(std::string_view word, int32_t score = 0, std::string_view annotation = "");
    [[nodiscard]] bool contains(std::string_view word) const;

    [[nodiscard]] std::vector<CandidateEntry> lookup_prefix(std::string_view prefix, size_t max_results = 20) const;
    [[nodiscard]] std::vector<CandidateEntry> lookup_exact(std::string_view word) const;
    [[nodiscard]] std::vector<CandidateEntry> lookup_fuzzy(std::string_view query, size_t max_results = 20) const;

    [[nodiscard]] size_t word_count() const noexcept;
    [[nodiscard]] size_t node_count() const noexcept;
    void clear() noexcept;

    [[nodiscard]] std::vector<uint8_t> serialize() const;
    bool deserialize(std::span<const uint8_t> bytes);
    bool save_to_file(const std::string& path) const;
    bool load_from_file(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

enum class InputMode : uint8_t {
    Compose,
    Dictionary,
    Bypass
};

class ImeEngine {
public:
    ImeEngine();
    ~ImeEngine();

    ImeEngine(const ImeEngine&) = delete;
    ImeEngine& operator=(const ImeEngine&) = delete;
    ImeEngine(ImeEngine&&) noexcept;
    ImeEngine& operator=(ImeEngine&&) noexcept;

    bool feed_key(KeySym sym, std::string* committed_text = nullptr);

    std::string commit_selected();
    std::string commit_preedit();
    void clear_preedit() noexcept;

    [[nodiscard]] std::string_view preedit_text() const noexcept;
    [[nodiscard]] size_t cursor_position() const noexcept;
    [[nodiscard]] bool is_active() const noexcept;

    [[nodiscard]] InputMode mode() const noexcept;
    void set_mode(InputMode mode) noexcept;

    [[nodiscard]] ComposeEngine& compose() noexcept;
    [[nodiscard]] const ComposeEngine& compose() const noexcept;

    [[nodiscard]] CandidateManager& candidates() noexcept;
    [[nodiscard]] const CandidateManager& candidates() const noexcept;

    [[nodiscard]] DictionaryTrie& dictionary() noexcept;
    [[nodiscard]] const DictionaryTrie& dictionary() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bro::ime
