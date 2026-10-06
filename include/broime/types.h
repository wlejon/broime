#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace bro::ime {

using KeySym = uint32_t;

namespace keysyms {
// Special keys
inline constexpr KeySym VoidSymbol       = 0xFFFFFF;
inline constexpr KeySym BackSpace        = 0xFF08;
inline constexpr KeySym Tab              = 0xFF09;
inline constexpr KeySym Return           = 0xFF0D;
inline constexpr KeySym Escape           = 0xFF1B;
inline constexpr KeySym Delete           = 0xFFFF;
inline constexpr KeySym Space            = 0x0020;
inline constexpr KeySym MultiKey         = 0xFF20; // Compose key

// Navigation
inline constexpr KeySym Home             = 0xFF50;
inline constexpr KeySym Left             = 0xFF51;
inline constexpr KeySym Up               = 0xFF52;
inline constexpr KeySym Right            = 0xFF53;
inline constexpr KeySym Down             = 0xFF54;
inline constexpr KeySym PageUp           = 0xFF55;
inline constexpr KeySym PageDown         = 0xFF56;
inline constexpr KeySym End              = 0xFF57;

// Dead keys (X11 / XKB standard range 0xFE50 - 0xFE6F)
inline constexpr KeySym DeadGrave        = 0xFE50;
inline constexpr KeySym DeadAcute        = 0xFE51;
inline constexpr KeySym DeadCircumflex   = 0xFE52;
inline constexpr KeySym DeadTilde        = 0xFE53;
inline constexpr KeySym DeadMacron       = 0xFE54;
inline constexpr KeySym DeadBreve        = 0xFE55;
inline constexpr KeySym DeadAboveDot     = 0xFE56;
inline constexpr KeySym DeadDiaeresis    = 0xFE57;
inline constexpr KeySym DeadAboveRing    = 0xFE58;
inline constexpr KeySym DeadDoubleAcute  = 0xFE59;
inline constexpr KeySym DeadCaron        = 0xFE5A;
inline constexpr KeySym DeadCedilla      = 0xFE5B;
inline constexpr KeySym DeadOgonek       = 0xFE5C;
inline constexpr KeySym DeadIota         = 0xFE5D;
inline constexpr KeySym DeadVoicedSound  = 0xFE5E;
inline constexpr KeySym DeadSemivoicedSound = 0xFE5F;
inline constexpr KeySym DeadBelowDot     = 0xFE60;
inline constexpr KeySym DeadHook         = 0xFE61;
inline constexpr KeySym DeadHorn         = 0xFE62;
inline constexpr KeySym DeadStroke       = 0xFE63;
} // namespace keysyms

KeySym keysym_from_name(std::string_view name);
std::string keysym_to_name(KeySym sym);
std::string keysym_to_utf8(KeySym sym);
KeySym keysym_from_utf8(std::string_view utf8);

struct ComposeSequence {
    std::vector<KeySym> keys;

    ComposeSequence() = default;
    ComposeSequence(std::initializer_list<KeySym> list) : keys(list) {}
    explicit ComposeSequence(std::vector<KeySym> k) : keys(std::move(k)) {}

    void push_back(KeySym k) { keys.push_back(k); }
    void clear() noexcept { keys.clear(); }
    [[nodiscard]] size_t size() const noexcept { return keys.size(); }
    [[nodiscard]] bool empty() const noexcept { return keys.empty(); }
    KeySym operator[](size_t index) const { return keys[index]; }
    KeySym& operator[](size_t index) { return keys[index]; }

    auto begin() noexcept { return keys.begin(); }
    auto end() noexcept { return keys.end(); }
    auto begin() const noexcept { return keys.begin(); }
    auto end() const noexcept { return keys.end(); }

    bool operator==(const ComposeSequence& other) const = default;
    auto operator<=>(const ComposeSequence& other) const = default;

    [[nodiscard]] std::string to_string() const;
    static ComposeSequence from_string(std::string_view spec);
};

struct ComposeResult {
    enum Status : uint8_t {
        None,
        Pending,
        Matched
    };

    Status status = None;
    std::string text;

    constexpr ComposeResult() noexcept = default;
    constexpr ComposeResult(Status s) noexcept : status(s) {}
    ComposeResult(Status s, std::string t) : status(s), text(std::move(t)) {}

    constexpr operator Status() const noexcept { return status; }
    constexpr bool operator==(Status s) const noexcept { return status == s; }
    bool operator==(const ComposeResult& other) const = default;

    [[nodiscard]] constexpr bool is_none() const noexcept { return status == None; }
    [[nodiscard]] constexpr bool is_pending() const noexcept { return status == Pending; }
    [[nodiscard]] constexpr bool is_matched() const noexcept { return status == Matched; }
};

struct CandidateEntry {
    std::string text;
    std::string annotation;
    std::string match_query;
    int32_t score = 0;
    uint32_t id = 0;

    bool operator==(const CandidateEntry& other) const = default;
};

struct AnchorRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] constexpr int left() const noexcept { return x; }
    [[nodiscard]] constexpr int top() const noexcept { return y; }
    [[nodiscard]] constexpr int right() const noexcept { return x + width; }
    [[nodiscard]] constexpr int bottom() const noexcept { return y + height; }
    [[nodiscard]] constexpr bool is_empty() const noexcept { return width <= 0 || height <= 0; }

    [[nodiscard]] constexpr bool contains(int px, int py) const noexcept {
        return px >= x && px < right() && py >= y && py < bottom();
    }

    [[nodiscard]] constexpr bool intersects(const AnchorRect& other) const noexcept {
        return x < other.right() && right() > other.x &&
               y < other.bottom() && bottom() > other.y;
    }

    constexpr bool operator==(const AnchorRect& other) const noexcept = default;
};

enum class AnchorDirection : uint8_t {
    Below,
    Above
};

struct PopupPlacement {
    AnchorRect rect;
    AnchorDirection direction = AnchorDirection::Below;
    bool clamped_x = false;
    bool clamped_y = false;

    constexpr bool operator==(const PopupPlacement& other) const noexcept = default;
};

struct CandidatePage {
    std::vector<CandidateEntry> entries;
    size_t page_index = 0;
    size_t total_pages = 0;
    size_t page_size = 9;
    size_t total_candidates = 0;
    size_t selected_index_in_page = 0;
    size_t selected_global_index = 0;

    [[nodiscard]] bool has_prev_page() const noexcept { return page_index > 0; }
    [[nodiscard]] bool has_next_page() const noexcept { return page_index + 1 < total_pages; }
    [[nodiscard]] const CandidateEntry* selected_candidate() const noexcept {
        if (selected_index_in_page < entries.size()) {
            return &entries[selected_index_in_page];
        }
        return nullptr;
    }

    bool operator==(const CandidatePage& other) const = default;
};

inline std::ostream& operator<<(std::ostream& os, const ComposeSequence& seq) {
    return os << seq.to_string();
}

inline std::ostream& operator<<(std::ostream& os, const ComposeResult& res) {
    switch (res.status) {
        case ComposeResult::None: return os << "ComposeResult::None";
        case ComposeResult::Pending: return os << "ComposeResult::Pending";
        case ComposeResult::Matched: return os << "ComposeResult::Matched(\"" << res.text << "\")";
    }
    return os << "ComposeResult(unknown)";
}

inline std::ostream& operator<<(std::ostream& os, const AnchorRect& r) {
    return os << "AnchorRect{" << r.x << ", " << r.y << ", " << r.width << ", " << r.height << "}";
}

} // namespace bro::ime
