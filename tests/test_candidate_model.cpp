#include <broime/ime.h>
#include "check.h"

using namespace bro::ime;

int main() {
    CandidateManager mgr;

    // 1. Empty state
    CHECK(!mgr.has_candidates());
    CHECK_EQ(mgr.candidate_count(), 0u);
    CHECK_EQ(mgr.total_pages(), 0u);
    CHECK_EQ(mgr.current_page_index(), 0u);
    CHECK_EQ(mgr.selected_candidate(), nullptr);
    CHECK(!mgr.select_next());
    CHECK(!mgr.select_prev());
    CHECK(!mgr.next_page());
    CHECK(!mgr.prev_page());

    // 2. Setting candidates and pagination
    std::vector<CandidateEntry> entries;
    for (int i = 0; i < 23; ++i) {
        CandidateEntry entry;
        entry.text = "item_" + std::to_string(i);
        entry.annotation = "annot_" + std::to_string(i);
        entry.score = 100 - i;
        entry.id = static_cast<uint32_t>(i);
        entries.push_back(std::move(entry));
    }

    mgr.set_candidates(entries);
    mgr.set_page_size(5);

    CHECK(mgr.has_candidates());
    CHECK_EQ(mgr.candidate_count(), 23u);
    CHECK_EQ(mgr.page_size(), 5u);
    // 23 items / 5 = 5 pages (5, 5, 5, 5, 3)
    CHECK_EQ(mgr.total_pages(), 5u);
    CHECK_EQ(mgr.current_page_index(), 0u);
    CHECK_EQ(mgr.selected_index(), 0u);
    REQUIRE(mgr.selected_candidate() != nullptr);
    CHECK_EQ(mgr.selected_candidate()->text, "item_0");

    // Page 0 content
    CandidatePage page0 = mgr.current_page();
    CHECK_EQ(page0.page_index, 0u);
    CHECK_EQ(page0.total_pages, 5u);
    CHECK_EQ(page0.entries.size(), 5u);
    CHECK_EQ(page0.entries[0].text, "item_0");
    CHECK_EQ(page0.entries[4].text, "item_4");
    CHECK(!page0.has_prev_page());
    CHECK(page0.has_next_page());

    // 3. Selection movement
    CHECK(mgr.select_next());
    CHECK_EQ(mgr.selected_index(), 1u);
    CHECK_EQ(mgr.current_page_index(), 0u);

    // Jump to next page
    CHECK(mgr.next_page());
    CHECK_EQ(mgr.current_page_index(), 1u);
    CHECK_EQ(mgr.selected_index(), 5u);
    CHECK_EQ(mgr.selected_candidate()->text, "item_5");

    // Jump to last page (page 4)
    CHECK(mgr.prev_page());
    CHECK_EQ(mgr.current_page_index(), 0u);
    CHECK(mgr.prev_page()); // wraps to last page
    CHECK_EQ(mgr.current_page_index(), 4u);
    CHECK_EQ(mgr.selected_index(), 20u);

    // Page 4 has 3 items
    CandidatePage page4 = mgr.current_page();
    CHECK_EQ(page4.entries.size(), 3u);
    CHECK_EQ(page4.entries[0].text, "item_20");
    CHECK_EQ(page4.entries[2].text, "item_22");
    CHECK(page4.has_prev_page());
    CHECK(!page4.has_next_page());

    // 4. Page-relative index selection (for number keys 1..5)
    CHECK(mgr.select_page_index(2)); // item_22 on page 4
    CHECK_EQ(mgr.selected_index(), 22u);
    CHECK_EQ(mgr.selected_candidate()->text, "item_22");

    // Invalid page-relative index beyond page size or list bounds
    CHECK(!mgr.select_page_index(3)); // only 3 items on page 4 (0, 1, 2)
    CHECK_EQ(mgr.selected_index(), 22u);

    // 5. Selection wrap-around
    CHECK(mgr.select_next()); // wraps from 22 to 0
    CHECK_EQ(mgr.selected_index(), 0u);
    CHECK(mgr.select_prev()); // wraps from 0 to 22
    CHECK_EQ(mgr.selected_index(), 22u);

    // 6. Fuzzy filtering with brosearch
    mgr.filter_fuzzy("item_1");
    // Should match item_1, item_10..item_19
    CHECK(mgr.has_candidates());
    CHECK_LE(mgr.candidate_count(), 23u);
    CHECK_GE(mgr.candidate_count(), 10u);

    // Reset filter
    mgr.filter_fuzzy("");
    CHECK_EQ(mgr.candidate_count(), 23u);

    return bstest::finish("test_candidate_model");
}
