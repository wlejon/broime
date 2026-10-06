#include <broime/ime.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace bro::ime {

namespace {

struct ComposeNode {
    std::unordered_map<KeySym, std::unique_ptr<ComposeNode>> children;
    std::string result_utf8;
    bool is_terminal = false;
};

std::string parse_quoted_string(std::string_view s, size_t& chars_consumed) {
    chars_consumed = 0;
    if (s.empty() || s.front() != '"') {
        return {};
    }
    std::string out;
    size_t i = 1;
    bool closed = false;
    while (i < s.size()) {
        if (s[i] == '"') {
            closed = true;
            ++i;
            break;
        }
        if (s[i] == '\\' && i + 1 < s.size()) {
            char esc = s[i + 1];
            switch (esc) {
                case 'n': out.push_back('\n'); i += 2; break;
                case 't': out.push_back('\t'); i += 2; break;
                case 'r': out.push_back('\r'); i += 2; break;
                case '\\': out.push_back('\\'); i += 2; break;
                case '"': out.push_back('"'); i += 2; break;
                case 'x': {
                    if (i + 3 < s.size()) {
                        std::string hex_str(s.substr(i + 2, 2));
                        char* end = nullptr;
                        long val = std::strtol(hex_str.c_str(), &end, 16);
                        if (end == hex_str.c_str() + 2) {
                            out.push_back(static_cast<char>(val));
                            i += 4;
                            break;
                        }
                    }
                    out.push_back(esc);
                    i += 2;
                    break;
                }
                default:
                    out.push_back(esc);
                    i += 2;
                    break;
            }
        } else {
            out.push_back(s[i]);
            ++i;
        }
    }
    if (!closed) return {};
    chars_consumed = i;
    return out;
}

std::string_view trim_whitespace(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

} // namespace

struct ComposeEngine::Impl {
    std::unique_ptr<ComposeNode> root = std::make_unique<ComposeNode>();
    ComposeNode* current_node = nullptr;
    ComposeSequence current_seq;
    std::string last_match;
    size_t rule_count = 0;
    bool allow_ascii_accents = true;

    void add_rule_internal(const ComposeSequence& seq, std::string_view utf8_output) {
        if (seq.empty() || utf8_output.empty()) return;
        ComposeNode* node = root.get();
        for (KeySym sym : seq) {
            auto& child = node->children[sym];
            if (!child) {
                child = std::make_unique<ComposeNode>();
            }
            node = child.get();
        }
        if (!node->is_terminal) {
            ++rule_count;
        }
        node->is_terminal = true;
        node->result_utf8 = std::string(utf8_output);
    }

    void reset_state() noexcept {
        current_node = root.get();
        current_seq.clear();
    }
};

ComposeEngine::ComposeEngine() : impl_(std::make_unique<Impl>()) {
    impl_->reset_state();
    load_builtin_rules();
}

ComposeEngine::~ComposeEngine() = default;
ComposeEngine::ComposeEngine(ComposeEngine&&) noexcept = default;
ComposeEngine& ComposeEngine::operator=(ComposeEngine&&) noexcept = default;

void ComposeEngine::clear_rules() noexcept {
    impl_->root = std::make_unique<ComposeNode>();
    impl_->rule_count = 0;
    impl_->reset_state();
    impl_->last_match.clear();
}

void ComposeEngine::add_rule(const ComposeSequence& seq, std::string_view utf8_output) {
    impl_->add_rule_internal(seq, utf8_output);
}

void ComposeEngine::add_rule(std::string_view sequence_spec, std::string_view utf8_output) {
    ComposeSequence seq = ComposeSequence::from_string(sequence_spec);
    add_rule(seq, utf8_output);
}

bool ComposeEngine::load_xcompose_string(std::string_view content) {
    size_t pos = 0;
    bool any_added = false;

    while (pos < content.size()) {
        size_t next_line = content.find('\n', pos);
        std::string_view line = (next_line == std::string_view::npos)
            ? content.substr(pos)
            : content.substr(pos, next_line - pos);
        pos = (next_line == std::string_view::npos) ? content.size() : next_line + 1;

        line = trim_whitespace(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        // Directives like "include"
        if (line.starts_with("include")) {
            continue;
        }

        size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }

        std::string_view lhs = trim_whitespace(line.substr(0, colon));
        std::string_view rhs = trim_whitespace(line.substr(colon + 1));

        // Right hand side starts with string literal "..."
        if (rhs.empty() || rhs.front() != '"') {
            continue;
        }
        size_t consumed = 0;
        std::string parsed_val = parse_quoted_string(rhs, consumed);
        if (consumed == 0) {
            continue;
        }

        ComposeSequence seq = ComposeSequence::from_string(lhs);
        if (!seq.empty() && !parsed_val.empty()) {
            add_rule(seq, parsed_val);
            any_added = true;
        }
    }

    return any_added;
}

bool ComposeEngine::load_xcompose_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return load_xcompose_string(ss.str());
}

ComposeResult ComposeEngine::feed(KeySym sym) {
    if (!impl_->current_node) {
        impl_->current_node = impl_->root.get();
    }

    auto it = impl_->current_node->children.find(sym);
    if (it == impl_->current_node->children.end()) {
        // Not a continuation
        if (impl_->current_node != impl_->root.get()) {
            // Sequence broken. Check if this key starts a fresh sequence at root.
            impl_->reset_state();
            auto root_it = impl_->root->children.find(sym);
            if (root_it != impl_->root->children.end()) {
                impl_->current_node = root_it->second.get();
                impl_->current_seq.push_back(sym);
                if (impl_->current_node->is_terminal && impl_->current_node->children.empty()) {
                    std::string text = impl_->current_node->result_utf8;
                    impl_->last_match = text;
                    impl_->reset_state();
                    return ComposeResult(ComposeResult::Matched, std::move(text));
                }
                return ComposeResult(ComposeResult::Pending);
            }
        }
        impl_->reset_state();
        return ComposeResult(ComposeResult::None);
    }

    impl_->current_node = it->second.get();
    impl_->current_seq.push_back(sym);

    if (impl_->current_node->is_terminal && impl_->current_node->children.empty()) {
        std::string text = impl_->current_node->result_utf8;
        impl_->last_match = text;
        impl_->reset_state();
        return ComposeResult(ComposeResult::Matched, std::move(text));
    }

    return ComposeResult(ComposeResult::Pending);
}

void ComposeEngine::reset() noexcept {
    impl_->reset_state();
}

bool ComposeEngine::is_composing() const noexcept {
    return impl_->current_node != impl_->root.get() && !impl_->current_seq.empty();
}

const ComposeSequence& ComposeEngine::current_sequence() const noexcept {
    return impl_->current_seq;
}

std::string_view ComposeEngine::matched_text() const noexcept {
    return impl_->last_match;
}

size_t ComposeEngine::rule_count() const noexcept {
    return impl_->rule_count;
}

bool ComposeEngine::allow_ascii_accents() const noexcept {
    return impl_->allow_ascii_accents;
}

void ComposeEngine::set_allow_ascii_accents(bool enable) noexcept {
    if (impl_->allow_ascii_accents == enable) return;
    impl_->allow_ascii_accents = enable;
    clear_rules();
    load_builtin_rules();
}

void ComposeEngine::load_builtin_rules() {
    using namespace keysyms;

    // Helper lambda for pairs
    auto add_accent = [this](KeySym dead_sym, KeySym ascii_sym,
                             const std::vector<std::pair<KeySym, std::string_view>>& table,
                             std::string_view base_char) {
        for (const auto& [letter, result] : table) {
            // dead key + letter
            add_rule({dead_sym, letter}, result);
            // Multi_key + accent + letter
            add_rule({MultiKey, ascii_sym, letter}, result);
            // ASCII dead key combination if enabled
            if (impl_->allow_ascii_accents) {
                add_rule({ascii_sym, letter}, result);
            }
        }
        // Self combination (e.g. ' + ' -> ')
        add_rule({dead_sym, ascii_sym}, base_char);
        add_rule({dead_sym, Space}, base_char);
        add_rule({dead_sym, dead_sym}, base_char);
        if (impl_->allow_ascii_accents) {
            add_rule({ascii_sym, ascii_sym}, base_char);
            add_rule({ascii_sym, Space}, base_char);
        }
    };

    // Acute: '
    add_accent(DeadAcute, '\'', {
        {'a', "á"}, {'e', "é"}, {'i', "í"}, {'o', "ó"}, {'u', "ú"}, {'y', "ý"},
        {'c', "ć"}, {'n', "ń"}, {'s', "ś"}, {'z', "ź"},
        {'A', "Á"}, {'E', "É"}, {'I', "Í"}, {'O', "Ó"}, {'U', "Ú"}, {'Y', "Ý"},
        {'C', "Ć"}, {'N', "Ń"}, {'S', "Ś"}, {'Z', "Ź"}
    }, "'");

    // Grave: `
    add_accent(DeadGrave, '`', {
        {'a', "à"}, {'e', "è"}, {'i', "ì"}, {'o', "ò"}, {'u', "ù"},
        {'A', "À"}, {'E', "È"}, {'I', "Ì"}, {'O', "Ò"}, {'U', "Ù"}
    }, "`");

    // Circumflex: ^
    add_accent(DeadCircumflex, '^', {
        {'a', "â"}, {'e', "ê"}, {'i', "î"}, {'o', "ô"}, {'u', "û"},
        {'c', "ĉ"}, {'g', "ĝ"}, {'h', "ĥ"}, {'j', "ĵ"}, {'s', "ŝ"}, {'w', "ŵ"}, {'y', "ŷ"},
        {'A', "Â"}, {'E', "Ê"}, {'I', "Î"}, {'O', "Ô"}, {'U', "Û"},
        {'C', "Ĉ"}, {'G', "Ĝ"}, {'H', "Ĥ"}, {'J', "Ĵ"}, {'S', "Ŝ"}, {'W', "Ŵ"}, {'Y', "Ŷ"}
    }, "^");

    // Tilde: ~
    add_accent(DeadTilde, '~', {
        {'a', "ã"}, {'e', "ẽ"}, {'i', "ĩ"}, {'o', "õ"}, {'u', "ũ"}, {'n', "ñ"},
        {'A', "Ã"}, {'E', "Ẽ"}, {'I', "Ĩ"}, {'O', "Õ"}, {'U', "Ũ"}, {'N', "Ñ"}
    }, "~");

    // Diaeresis / Umlaut: "
    add_accent(DeadDiaeresis, '"', {
        {'a', "ä"}, {'e', "ë"}, {'i', "ï"}, {'o', "ö"}, {'u', "ü"}, {'y', "ÿ"},
        {'A', "Ä"}, {'E', "Ë"}, {'I', "Ï"}, {'O', "Ö"}, {'U', "Ü"}, {'Y', "Ÿ"}
    }, "\"");

    // Cedilla: ,
    add_accent(DeadCedilla, ',', {
        {'c', "ç"}, {'s', "ş"}, {'t', "ţ"},
        {'C', "Ç"}, {'S', "Ş"}, {'T', "Ţ"}
    }, "¸");

    // Above ring: å, ů
    add_rule({DeadAboveRing, 'a'}, "å");
    add_rule({DeadAboveRing, 'u'}, "ů");
    add_rule({DeadAboveRing, 'A'}, "Å");
    add_rule({DeadAboveRing, 'U'}, "Ů");
    add_rule({DeadAboveRing, Space}, "°");
    add_rule({DeadAboveRing, DeadAboveRing}, "°");
    add_rule({MultiKey, 'o', 'a'}, "å");
    add_rule({MultiKey, 'o', 'A'}, "Å");

    // Caron: č, š, ž, etc.
    const std::vector<std::pair<KeySym, std::string_view>> caron_table = {
        {'c', "č"}, {'d', "ď"}, {'e', "ě"}, {'l', "ľ"}, {'n', "ň"},
        {'r', "ř"}, {'s', "š"}, {'t', "ť"}, {'z', "ž"},
        {'C', "Č"}, {'D', "Ď"}, {'E', "Ě"}, {'L', "Ľ"}, {'N', "Ň"},
        {'R', "Ř"}, {'S', "Š"}, {'T', "Ť"}, {'Z', "Ž"}
    };
    for (const auto& [letter, result] : caron_table) {
        add_rule({DeadCaron, letter}, result);
        add_rule({MultiKey, 'v', letter}, result);
        add_rule({MultiKey, 'c', letter}, result);
    }

    // Macron
    const std::vector<std::pair<KeySym, std::string_view>> macron_table = {
        {'a', "ā"}, {'e', "ē"}, {'i', "ī"}, {'o', "ō"}, {'u', "ū"},
        {'A', "Ā"}, {'E', "Ē"}, {'I', "Ī"}, {'O', "Ō"}, {'U', "Ū"}
    };
    for (const auto& [letter, result] : macron_table) {
        add_rule({DeadMacron, letter}, result);
        add_rule({MultiKey, '_', letter}, result);
        add_rule({MultiKey, '-', letter}, result);
    }

    // Ogonek
    const std::vector<std::pair<KeySym, std::string_view>> ogonek_table = {
        {'a', "ą"}, {'e', "ę"}, {'i', "į"}, {'u', "ų"},
        {'A', "Ą"}, {'E', "Ę"}, {'I', "Į"}, {'U', "Ų"}
    };
    for (const auto& [letter, result] : ogonek_table) {
        add_rule({DeadOgonek, letter}, result);
    }

    // Stroke
    const std::vector<std::pair<KeySym, std::string_view>> stroke_table = {
        {'d', "đ"}, {'h', "ħ"}, {'l', "ł"}, {'o', "ø"}, {'t', "ŧ"},
        {'D', "Đ"}, {'H', "Ħ"}, {'L', "Ł"}, {'O', "Ø"}, {'T', "Ŧ"}
    };
    for (const auto& [letter, result] : stroke_table) {
        add_rule({DeadStroke, letter}, result);
        add_rule({MultiKey, '/', letter}, result);
    }

    // Common Multi_key symbols
    add_rule({MultiKey, 'c', 'o'}, "©");
    add_rule({MultiKey, 'C', 'O'}, "©");
    add_rule({MultiKey, 'o', 'c'}, "©");
    add_rule({MultiKey, 'O', 'C'}, "©");

    add_rule({MultiKey, 'r', 'o'}, "®");
    add_rule({MultiKey, 'R', 'O'}, "®");
    add_rule({MultiKey, 'o', 'r'}, "®");
    add_rule({MultiKey, 'O', 'R'}, "®");

    add_rule({MultiKey, 't', 'm'}, "™");
    add_rule({MultiKey, 'T', 'M'}, "™");

    add_rule({MultiKey, 's', 's'}, "ß");
    add_rule({MultiKey, 'a', 'e'}, "æ");
    add_rule({MultiKey, 'A', 'E'}, "Æ");
    add_rule({MultiKey, 'o', 'e'}, "œ");
    add_rule({MultiKey, 'O', 'E'}, "Œ");

    add_rule({MultiKey, 'c', '/'}, "¢");
    add_rule({MultiKey, '/', 'c'}, "¢");
    add_rule({MultiKey, 'y', '='}, "¥");
    add_rule({MultiKey, '=', 'y'}, "¥");
    add_rule({MultiKey, 'e', '='}, "€");
    add_rule({MultiKey, '=', 'e'}, "€");
    add_rule({MultiKey, 'l', '-'}, "£");
    add_rule({MultiKey, '-', 'l'}, "£");

    add_rule({MultiKey, '<', '<'}, "«");
    add_rule({MultiKey, '>', '>'}, "»");
    add_rule({MultiKey, '!', '!'}, "¡");
    add_rule({MultiKey, '?', '?'}, "¿");

    add_rule({MultiKey, '-', '>'}, "→");
    add_rule({MultiKey, '<', '-'}, "←");
    add_rule({MultiKey, '=', '>'}, "⇒");
    add_rule({MultiKey, '<', '='}, "⇐");

    add_rule({MultiKey, '.', '.'}, "…");
    add_rule({MultiKey, '-', '-'}, "–");
    add_rule({MultiKey, '-', '-', '-'}, "—");

    add_rule({MultiKey, 'x', 'x'}, "×");
    add_rule({MultiKey, ':', '-'}, "÷");
    add_rule({MultiKey, '1', '2'}, "½");
    add_rule({MultiKey, '1', '4'}, "¼");
    add_rule({MultiKey, '3', '4'}, "¾");
    add_rule({MultiKey, '0', '^'}, "°");
    add_rule({MultiKey, '+', '-'}, "±");
    add_rule({MultiKey, 'm', 'u'}, "µ");
    add_rule({MultiKey, 's', 'o'}, "§");
    add_rule({MultiKey, 'p', '!'}, "¶");
    add_rule({MultiKey, '<', '3'}, "♥");
}

} // namespace bro::ime
