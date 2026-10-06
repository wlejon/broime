#include <broime/ime.h>

#include <cctype>
#include <string>
#include <utility>

namespace bro::ime {

struct ImeEngine::Impl {
    ComposeEngine compose;
    CandidateManager candidates;
    DictionaryTrie dictionary;

    InputMode mode = InputMode::Dictionary;
    std::string preedit_buffer;
    size_t cursor_pos = 0;

    void update_candidates_from_preedit() {
        if (preedit_buffer.empty()) {
            candidates.clear();
            return;
        }
        auto results = dictionary.lookup_prefix(preedit_buffer);
        candidates.set_candidates(std::move(results));
    }
};

ImeEngine::ImeEngine() : impl_(std::make_unique<Impl>()) {}
ImeEngine::~ImeEngine() = default;
ImeEngine::ImeEngine(ImeEngine&&) noexcept = default;
ImeEngine& ImeEngine::operator=(ImeEngine&&) noexcept = default;

bool ImeEngine::feed_key(KeySym sym, std::string* committed_text) {
    if (committed_text) {
        committed_text->clear();
    }

    if (impl_->mode == InputMode::Bypass) {
        return false;
    }

    // 1. If currently in a compose sequence, feed compose engine first
    if (impl_->compose.is_composing()) {
        auto res = impl_->compose.feed(sym);
        if (res.is_matched()) {
            if (committed_text) {
                *committed_text = res.text;
            }
            impl_->preedit_buffer.clear();
            impl_->cursor_pos = 0;
            return true;
        }
        if (res.is_pending()) {
            impl_->preedit_buffer = impl_->compose.current_sequence().to_string();
            impl_->cursor_pos = impl_->preedit_buffer.size();
            return true;
        }
        // Sequence failed / cancelled
        impl_->preedit_buffer.clear();
        impl_->cursor_pos = 0;
        return false;
    }

    // 2. Check if key initiates a compose sequence (e.g. MultiKey or dead key)
    bool is_compose_starter = (sym == keysyms::MultiKey) ||
                              (sym >= keysyms::DeadGrave && sym <= keysyms::DeadStroke);
    if (is_compose_starter || impl_->mode == InputMode::Compose) {
        auto res = impl_->compose.feed(sym);
        if (res.is_matched()) {
            if (committed_text) {
                *committed_text = res.text;
            }
            return true;
        }
        if (res.is_pending()) {
            impl_->preedit_buffer = impl_->compose.current_sequence().to_string();
            impl_->cursor_pos = impl_->preedit_buffer.size();
            return true;
        }
        if (is_compose_starter) {
            return true;
        }
    }

    // 3. Navigation and candidate handling in Dictionary mode
    if (impl_->mode == InputMode::Dictionary) {
        // Escape cancels preedit and candidate window
        if (sym == keysyms::Escape) {
            if (!impl_->preedit_buffer.empty() || impl_->candidates.has_candidates()) {
                clear_preedit();
                return true;
            }
            return false;
        }

        // Backspace deletes preedit char
        if (sym == keysyms::BackSpace) {
            if (!impl_->preedit_buffer.empty()) {
                impl_->preedit_buffer.pop_back();
                impl_->cursor_pos = impl_->preedit_buffer.size();
                impl_->update_candidates_from_preedit();
                return true;
            }
            return false;
        }

        // Return commits raw preedit
        if (sym == keysyms::Return) {
            if (!impl_->preedit_buffer.empty()) {
                if (committed_text) {
                    *committed_text = commit_preedit();
                } else {
                    commit_preedit();
                }
                return true;
            }
            return false;
        }

        // Candidate navigation when candidates are present
        if (impl_->candidates.has_candidates()) {
            // Space commits selected candidate
            if (sym == keysyms::Space) {
                if (committed_text) {
                    *committed_text = commit_selected();
                } else {
                    commit_selected();
                }
                return true;
            }

            // Number keys 1..9 select page candidate
            if (sym >= '1' && sym <= '9') {
                size_t page_idx = static_cast<size_t>(sym - '1');
                if (impl_->candidates.select_page_index(page_idx)) {
                    if (committed_text) {
                        *committed_text = commit_selected();
                    } else {
                        commit_selected();
                    }
                    return true;
                }
            }

            // Arrow down / next candidate
            if (sym == keysyms::Down) {
                return impl_->candidates.select_next();
            }

            // Arrow up / prev candidate
            if (sym == keysyms::Up) {
                return impl_->candidates.select_prev();
            }

            // PageDown / Next page
            if (sym == keysyms::PageDown) {
                return impl_->candidates.next_page();
            }

            // PageUp / Prev page
            if (sym == keysyms::PageUp) {
                return impl_->candidates.prev_page();
            }
        }

        // Printable ASCII character typed into preedit buffer
        if (sym >= 0x20 && sym <= 0x7E) {
            char c = static_cast<char>(sym);
            if (std::isalpha(static_cast<unsigned char>(c)) || (!impl_->preedit_buffer.empty() && std::isdigit(static_cast<unsigned char>(c)))) {
                impl_->preedit_buffer.push_back(c);
                impl_->cursor_pos = impl_->preedit_buffer.size();
                impl_->update_candidates_from_preedit();
                return true;
            }
        }
    }

    return false;
}

std::string ImeEngine::commit_selected() {
    std::string text;
    const auto* cand = impl_->candidates.selected_candidate();
    if (cand) {
        text = cand->text;
    } else {
        text = impl_->preedit_buffer;
    }
    clear_preedit();
    return text;
}

std::string ImeEngine::commit_preedit() {
    std::string text = std::move(impl_->preedit_buffer);
    clear_preedit();
    return text;
}

void ImeEngine::clear_preedit() noexcept {
    impl_->preedit_buffer.clear();
    impl_->cursor_pos = 0;
    impl_->candidates.clear();
    impl_->compose.reset();
}

std::string_view ImeEngine::preedit_text() const noexcept {
    return impl_->preedit_buffer;
}

size_t ImeEngine::cursor_position() const noexcept {
    return impl_->cursor_pos;
}

bool ImeEngine::is_active() const noexcept {
    return !impl_->preedit_buffer.empty() ||
           impl_->compose.is_composing() ||
           impl_->candidates.has_candidates();
}

InputMode ImeEngine::mode() const noexcept {
    return impl_->mode;
}

void ImeEngine::set_mode(InputMode mode) noexcept {
    if (impl_->mode != mode) {
        clear_preedit();
        impl_->mode = mode;
    }
}

ComposeEngine& ImeEngine::compose() noexcept {
    return impl_->compose;
}

const ComposeEngine& ImeEngine::compose() const noexcept {
    return impl_->compose;
}

CandidateManager& ImeEngine::candidates() noexcept {
    return impl_->candidates;
}

const CandidateManager& ImeEngine::candidates() const noexcept {
    return impl_->candidates;
}

DictionaryTrie& ImeEngine::dictionary() noexcept {
    return impl_->dictionary;
}

const DictionaryTrie& ImeEngine::dictionary() const noexcept {
    return impl_->dictionary;
}

} // namespace bro::ime
