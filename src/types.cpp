#include <broime/types.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace bro::ime {

namespace {

const std::unordered_map<std::string_view, KeySym>& named_keysyms_table() {
    static const std::unordered_map<std::string_view, KeySym> map = {
        {"Multi_key", keysyms::MultiKey},
        {"Compose", keysyms::MultiKey},
        {"BackSpace", keysyms::BackSpace},
        {"Tab", keysyms::Tab},
        {"Return", keysyms::Return},
        {"Enter", keysyms::Return},
        {"Escape", keysyms::Escape},
        {"Delete", keysyms::Delete},
        {"space", keysyms::Space},
        {"Home", keysyms::Home},
        {"Left", keysyms::Left},
        {"Up", keysyms::Up},
        {"Right", keysyms::Right},
        {"Down", keysyms::Down},
        {"Prior", keysyms::PageUp},
        {"Page_Up", keysyms::PageUp},
        {"PageUp", keysyms::PageUp},
        {"Next", keysyms::PageDown},
        {"Page_Down", keysyms::PageDown},
        {"PageDown", keysyms::PageDown},
        {"End", keysyms::End},

        // Dead keys
        {"dead_grave", keysyms::DeadGrave},
        {"dead_acute", keysyms::DeadAcute},
        {"dead_circumflex", keysyms::DeadCircumflex},
        {"dead_tilde", keysyms::DeadTilde},
        {"dead_macron", keysyms::DeadMacron},
        {"dead_breve", keysyms::DeadBreve},
        {"dead_abovedot", keysyms::DeadAboveDot},
        {"dead_diaeresis", keysyms::DeadDiaeresis},
        {"dead_abovering", keysyms::DeadAboveRing},
        {"dead_doubleacute", keysyms::DeadDoubleAcute},
        {"dead_caron", keysyms::DeadCaron},
        {"dead_cedilla", keysyms::DeadCedilla},
        {"dead_ogonek", keysyms::DeadOgonek},
        {"dead_iota", keysyms::DeadIota},
        {"dead_voiced_sound", keysyms::DeadVoicedSound},
        {"dead_semivoiced_sound", keysyms::DeadSemivoicedSound},
        {"dead_belowdot", keysyms::DeadBelowDot},
        {"dead_hook", keysyms::DeadHook},
        {"dead_horn", keysyms::DeadHorn},
        {"dead_stroke", keysyms::DeadStroke},

        // Common punctuation names in XCompose
        {"exclam", '!'},
        {"quotedbl", '"'},
        {"numbersign", '#'},
        {"dollar", '$'},
        {"percent", '%'},
        {"ampersand", '&'},
        {"apostrophe", '\''},
        {"quoteright", '\''},
        {"quoteleft", '`'},
        {"parenleft", '('},
        {"parenright", ')'},
        {"asterisk", '*'},
        {"plus", '+'},
        {"comma", ','},
        {"minus", '-'},
        {"period", '.'},
        {"slash", '/'},
        {"colon", ':'},
        {"semicolon", ';'},
        {"less", '<'},
        {"equal", '='},
        {"greater", '>'},
        {"question", '?'},
        {"at", '@'},
        {"bracketleft", '['},
        {"backslash", '\\'},
        {"bracketright", ']'},
        {"asciicircum", '^'},
        {"underscore", '_'},
        {"grave", '`'},
        {"braceleft", '{'},
        {"bar", '|'},
        {"braceright", '}'},
        {"asciitilde", '~'},
        {"tilde", '~'}
    };
    return map;
}

std::string encode_utf8(uint32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

uint32_t decode_utf8(std::string_view utf8) {
    if (utf8.empty()) return 0;
    const auto u0 = static_cast<uint8_t>(utf8[0]);
    if ((u0 & 0x80) == 0) {
        return u0;
    }
    if ((u0 & 0xE0) == 0xC0 && utf8.size() >= 2) {
        return ((u0 & 0x1F) << 6) | (static_cast<uint8_t>(utf8[1]) & 0x3F);
    }
    if ((u0 & 0xF0) == 0xE0 && utf8.size() >= 3) {
        return ((u0 & 0x0F) << 12) |
               ((static_cast<uint8_t>(utf8[1]) & 0x3F) << 6) |
               (static_cast<uint8_t>(utf8[2]) & 0x3F);
    }
    if ((u0 & 0xF8) == 0xF0 && utf8.size() >= 4) {
        return ((u0 & 0x07) << 18) |
               ((static_cast<uint8_t>(utf8[1]) & 0x3F) << 12) |
               ((static_cast<uint8_t>(utf8[2]) & 0x3F) << 6) |
               (static_cast<uint8_t>(utf8[3]) & 0x3F);
    }
    return 0;
}

} // namespace

KeySym keysym_from_name(std::string_view name) {
    if (name.empty()) return keysyms::VoidSymbol;

    // Check named table
    const auto& table = named_keysyms_table();
    auto it = table.find(name);
    if (it != table.end()) {
        return it->second;
    }

    // Single character ASCII e.g. "a", "A", "7"
    if (name.size() == 1) {
        return static_cast<uint8_t>(name[0]);
    }

    // Unicode format e.g. "U00E9", "u00e9", "U+00E9"
    if ((name.front() == 'U' || name.front() == 'u')) {
        size_t offset = 1;
        if (name.size() > 2 && name[1] == '+') offset = 2;
        std::string_view hex_part = name.substr(offset);
        uint32_t cp = 0;
        auto res = std::from_chars(hex_part.data(), hex_part.data() + hex_part.size(), cp, 16);
        if (res.ec == std::errc{} && res.ptr == hex_part.data() + hex_part.size()) {
            if (cp >= 0x0100 && cp <= 0x10FFFF) {
                return 0x01000000u | cp;
            }
            return cp;
        }
    }

    // Hex integer format e.g. "0x00E9" or "0xFF20"
    if (name.size() > 2 && name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
        std::string_view hex_part = name.substr(2);
        uint32_t val = 0;
        auto res = std::from_chars(hex_part.data(), hex_part.data() + hex_part.size(), val, 16);
        if (res.ec == std::errc{} && res.ptr == hex_part.data() + hex_part.size()) {
            return val;
        }
    }

    return keysyms::VoidSymbol;
}

namespace {

struct CanonicalEntry {
    KeySym sym;
    const char* name;
};

const CanonicalEntry kCanonicalNames[] = {
    {keysyms::MultiKey, "Multi_key"},
    {keysyms::BackSpace, "BackSpace"},
    {keysyms::Tab, "Tab"},
    {keysyms::Return, "Return"},
    {keysyms::Escape, "Escape"},
    {keysyms::Delete, "Delete"},
    {keysyms::Space, "space"},
    {keysyms::Home, "Home"},
    {keysyms::Left, "Left"},
    {keysyms::Up, "Up"},
    {keysyms::Right, "Right"},
    {keysyms::Down, "Down"},
    {keysyms::PageUp, "Page_Up"},
    {keysyms::PageDown, "Page_Down"},
    {keysyms::End, "End"},

    // Dead keys
    {keysyms::DeadGrave, "dead_grave"},
    {keysyms::DeadAcute, "dead_acute"},
    {keysyms::DeadCircumflex, "dead_circumflex"},
    {keysyms::DeadTilde, "dead_tilde"},
    {keysyms::DeadMacron, "dead_macron"},
    {keysyms::DeadBreve, "dead_breve"},
    {keysyms::DeadAboveDot, "dead_abovedot"},
    {keysyms::DeadDiaeresis, "dead_diaeresis"},
    {keysyms::DeadAboveRing, "dead_abovering"},
    {keysyms::DeadDoubleAcute, "dead_doubleacute"},
    {keysyms::DeadCaron, "dead_caron"},
    {keysyms::DeadCedilla, "dead_cedilla"},
    {keysyms::DeadOgonek, "dead_ogonek"},
    {keysyms::DeadIota, "dead_iota"},
    {keysyms::DeadVoicedSound, "dead_voiced_sound"},
    {keysyms::DeadSemivoicedSound, "dead_semivoiced_sound"},
    {keysyms::DeadBelowDot, "dead_belowdot"},
    {keysyms::DeadHook, "dead_hook"},
    {keysyms::DeadHorn, "dead_horn"},
    {keysyms::DeadStroke, "dead_stroke"},

    // Punctuation
    {'\'', "apostrophe"},
    {'`', "grave"},
    {'~', "asciitilde"},
    {'^', "asciicircum"},
    {'"', "quotedbl"},
    {'!', "exclam"},
    {'#', "numbersign"},
    {'$', "dollar"},
    {'%', "percent"},
    {'&', "ampersand"},
    {'(', "parenleft"},
    {')', "parenright"},
    {'*', "asterisk"},
    {'+', "plus"},
    {',', "comma"},
    {'-', "minus"},
    {'.', "period"},
    {'/', "slash"},
    {':', "colon"},
    {';', "semicolon"},
    {'<', "less"},
    {'=', "equal"},
    {'>', "greater"},
    {'?', "question"},
    {'@', "at"},
    {'[', "bracketleft"},
    {'\\', "backslash"},
    {']', "bracketright"},
    {'_', "underscore"},
    {'{', "braceleft"},
    {'|', "bar"},
    {'}', "braceright"}
};

} // namespace

std::string keysym_to_name(KeySym sym) {
    for (const auto& entry : kCanonicalNames) {
        if (entry.sym == sym) {
            return std::string(entry.name);
        }
    }
    if ((sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z') || (sym >= '0' && sym <= '9')) {
        return std::string(1, static_cast<char>(sym));
    }
    if (sym >= 0x20 && sym <= 0x7E) {
        return std::string(1, static_cast<char>(sym));
    }
    char buf[16];
    if ((sym & 0xFF000000) == 0x01000000) {
        std::snprintf(buf, sizeof(buf), "U%04X", sym & 0x00FFFFFF);
    } else {
        std::snprintf(buf, sizeof(buf), "0x%04X", sym);
    }
    return std::string(buf);
}

std::string keysym_to_utf8(KeySym sym) {
    if (sym >= 0x20 && sym <= 0x7E) {
        return std::string(1, static_cast<char>(sym));
    }
    if (sym >= 0x00A0 && sym <= 0x00FF) {
        return encode_utf8(sym);
    }
    if ((sym & 0xFF000000) == 0x01000000) {
        return encode_utf8(sym & 0x00FFFFFF);
    }
    return {};
}

KeySym keysym_from_utf8(std::string_view utf8) {
    uint32_t cp = decode_utf8(utf8);
    if (cp == 0) return keysyms::VoidSymbol;
    if (cp <= 0x00FF) return cp;
    return 0x01000000u | cp;
}

std::string ComposeSequence::to_string() const {
    std::string out;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) out.push_back(' ');
        out.push_back('<');
        out += keysym_to_name(keys[i]);
        out.push_back('>');
    }
    return out;
}

ComposeSequence ComposeSequence::from_string(std::string_view spec) {
    ComposeSequence seq;
    size_t i = 0;
    while (i < spec.size()) {
        while (i < spec.size() && std::isspace(static_cast<unsigned char>(spec[i]))) {
            ++i;
        }
        if (i >= spec.size()) break;
        if (spec[i] == '<') {
            size_t close = spec.find('>', i);
            if (close == std::string_view::npos) break;
            std::string_view sym_name = spec.substr(i + 1, close - i - 1);
            KeySym sym = keysym_from_name(sym_name);
            if (sym != keysyms::VoidSymbol) {
                seq.push_back(sym);
            }
            i = close + 1;
        } else {
            size_t token_end = i;
            while (token_end < spec.size() && !std::isspace(static_cast<unsigned char>(spec[token_end]))) {
                ++token_end;
            }
            std::string_view token = spec.substr(i, token_end - i);
            KeySym sym = keysym_from_name(token);
            if (sym != keysyms::VoidSymbol) {
                seq.push_back(sym);
            }
            i = token_end;
        }
    }
    return seq;
}

} // namespace bro::ime
