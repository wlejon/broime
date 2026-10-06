#include <broime/ime.h>

#include <brosearch/fuzzy.h>

#include <algorithm>
#include <string>
#include <vector>

namespace bro::ime {

CandidateManager::CandidateManager() = default;
CandidateManager::~CandidateManager() = default;

void CandidateManager::set_candidates(std::vector<CandidateEntry> entries) {
    original_candidates_ = entries;
    candidates_ = std::move(entries);
    selected_index_ = 0;
}

void CandidateManager::add_candidate(CandidateEntry entry) {
    original_candidates_.push_back(entry);
    candidates_.push_back(std::move(entry));
}

void CandidateManager::clear() noexcept {
    candidates_.clear();
    original_candidates_.clear();
    selected_index_ = 0;
}

size_t CandidateManager::candidate_count() const noexcept {
    return candidates_.size();
}

bool CandidateManager::has_candidates() const noexcept {
    return !candidates_.empty();
}

size_t CandidateManager::page_size() const noexcept {
    return page_size_;
}

void CandidateManager::set_page_size(size_t size) noexcept {
    page_size_ = (size == 0) ? 1 : size;
}

size_t CandidateManager::total_pages() const noexcept {
    if (candidates_.empty()) return 0;
    return (candidates_.size() + page_size_ - 1) / page_size_;
}

size_t CandidateManager::current_page_index() const noexcept {
    if (candidates_.empty()) return 0;
    return selected_index_ / page_size_;
}

size_t CandidateManager::selected_index() const noexcept {
    return selected_index_;
}

bool CandidateManager::select_next() {
    if (candidates_.empty()) return false;
    selected_index_ = (selected_index_ + 1) % candidates_.size();
    return true;
}

bool CandidateManager::select_prev() {
    if (candidates_.empty()) return false;
    if (selected_index_ == 0) {
        selected_index_ = candidates_.size() - 1;
    } else {
        --selected_index_;
    }
    return true;
}

bool CandidateManager::next_page() {
    if (candidates_.empty()) return false;
    size_t num_pages = total_pages();
    size_t cur = current_page_index();
    size_t next_p = (cur + 1) % num_pages;
    selected_index_ = next_p * page_size_;
    return true;
}

bool CandidateManager::prev_page() {
    if (candidates_.empty()) return false;
    size_t num_pages = total_pages();
    size_t cur = current_page_index();
    size_t prev_p = (cur == 0) ? (num_pages - 1) : (cur - 1);
    selected_index_ = prev_p * page_size_;
    return true;
}

bool CandidateManager::select_index(size_t global_index) {
    if (global_index < candidates_.size()) {
        selected_index_ = global_index;
        return true;
    }
    return false;
}

bool CandidateManager::select_page_index(size_t index_in_page) {
    size_t global = current_page_index() * page_size_ + index_in_page;
    if (global < candidates_.size()) {
        selected_index_ = global;
        return true;
    }
    return false;
}

CandidatePage CandidateManager::current_page() const {
    CandidatePage page;
    if (candidates_.empty()) {
        page.page_size = page_size_;
        return page;
    }

    page.page_index = current_page_index();
    page.total_pages = total_pages();
    page.page_size = page_size_;
    page.total_candidates = candidates_.size();
    page.selected_global_index = selected_index_;
    page.selected_index_in_page = selected_index_ % page_size_;

    size_t start = page.page_index * page_size_;
    size_t end = std::min(start + page_size_, candidates_.size());

    page.entries.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        page.entries.push_back(candidates_[i]);
    }

    return page;
}

const CandidateEntry* CandidateManager::selected_candidate() const noexcept {
    if (selected_index_ < candidates_.size()) {
        return &candidates_[selected_index_];
    }
    return nullptr;
}

const std::vector<CandidateEntry>& CandidateManager::all_candidates() const noexcept {
    return candidates_;
}

void CandidateManager::filter_fuzzy(std::string_view query) {
    if (query.empty()) {
        candidates_ = original_candidates_;
        selected_index_ = 0;
        return;
    }

    std::vector<std::string> item_strings;
    item_strings.reserve(original_candidates_.size());
    for (const auto& entry : original_candidates_) {
        // Match against text or annotation
        std::string full = entry.text;
        if (!entry.annotation.empty()) {
            full.push_back(' ');
            full += entry.annotation;
        }
        item_strings.push_back(std::move(full));
    }

    bro::search::FuzzyOptions opts;
    opts.case_mode = bro::search::FuzzyCase::Smart;
    bro::search::FuzzyQuery fq(query, opts);

    auto results = bro::search::fuzzy_filter(fq, std::span<const std::string>(item_strings));

    std::vector<CandidateEntry> filtered;
    filtered.reserve(results.size());
    for (const auto& r : results) {
        if (r.index < original_candidates_.size()) {
            CandidateEntry entry = original_candidates_[r.index];
            entry.score = r.score;
            filtered.push_back(std::move(entry));
        }
    }

    candidates_ = std::move(filtered);
    selected_index_ = 0;
}

PopupPlacement CandidateManager::compute_placement(
    const AnchorRect& cursor_rect,
    int popup_width,
    int popup_height,
    const AnchorRect& display_bounds,
    int margin
) {
    PopupPlacement placement;
    placement.rect.width = popup_width;
    placement.rect.height = popup_height;

    // 1. Vertical placement
    int y_below = cursor_rect.bottom() + margin;
    int y_above = cursor_rect.top() - margin - popup_height;

    bool fits_below = (y_below + popup_height <= display_bounds.bottom());
    bool fits_above = (y_above >= display_bounds.top());

    if (fits_below) {
        placement.direction = AnchorDirection::Below;
        placement.rect.y = y_below;
    } else if (fits_above) {
        placement.direction = AnchorDirection::Above;
        placement.rect.y = y_above;
    } else {
        // Neither fits without overflowing; choose the one with more available space
        int space_below = display_bounds.bottom() - y_below;
        int space_above = cursor_rect.top() - margin - display_bounds.top();

        if (space_below >= space_above) {
            placement.direction = AnchorDirection::Below;
            placement.rect.y = y_below;
            if (placement.rect.bottom() > display_bounds.bottom()) {
                placement.rect.y = display_bounds.bottom() - popup_height;
                placement.clamped_y = true;
            }
        } else {
            placement.direction = AnchorDirection::Above;
            placement.rect.y = y_above;
            if (placement.rect.top() < display_bounds.top()) {
                placement.rect.y = display_bounds.top();
                placement.clamped_y = true;
            }
        }
    }

    // Ensure y is not pushed above display top when clamped
    if (placement.rect.top() < display_bounds.top()) {
        placement.rect.y = display_bounds.top();
        placement.clamped_y = true;
    }

    // 2. Horizontal placement
    int x = cursor_rect.left();
    if (x + popup_width > display_bounds.right()) {
        x = display_bounds.right() - popup_width;
        placement.clamped_x = true;
    }
    if (x < display_bounds.left()) {
        x = display_bounds.left();
        placement.clamped_x = true;
    }

    placement.rect.x = x;
    return placement;
}

} // namespace bro::ime
