#include <broime/ime.h>
#include "check.h"

using namespace bro::ime;

int main() {
    AnchorRect display{0, 0, 1920, 1080};
    int popup_w = 200;
    int popup_h = 150;
    int margin = 5;

    // 1. Centered cursor: fits below nicely
    {
        AnchorRect cursor{500, 400, 2, 20};
        PopupPlacement p = CandidateManager::compute_placement(cursor, popup_w, popup_h, display, margin);
        CHECK_EQ(p.direction, AnchorDirection::Below);
        CHECK_EQ(p.rect.x, 500);
        CHECK_EQ(p.rect.y, 425); // 400 + 20 + 5
        CHECK_EQ(p.rect.width, popup_w);
        CHECK_EQ(p.rect.height, popup_h);
        CHECK(!p.clamped_x);
        CHECK(!p.clamped_y);
    }

    // 2. Cursor near bottom edge: flips above cursor
    {
        AnchorRect cursor{500, 1000, 2, 20}; // bottom is 1020, 1020 + 5 + 150 = 1175 > 1080
        PopupPlacement p = CandidateManager::compute_placement(cursor, popup_w, popup_h, display, margin);
        CHECK_EQ(p.direction, AnchorDirection::Above);
        CHECK_EQ(p.rect.x, 500);
        CHECK_EQ(p.rect.y, 1000 - 5 - 150); // 845
        CHECK(!p.clamped_x);
        CHECK(!p.clamped_y);
        CHECK_LE(p.rect.bottom(), cursor.top());
    }

    // 3. Cursor near right edge: clamps horizontally
    {
        AnchorRect cursor{1850, 400, 2, 20}; // 1850 + 200 = 2050 > 1920
        PopupPlacement p = CandidateManager::compute_placement(cursor, popup_w, popup_h, display, margin);
        CHECK_EQ(p.direction, AnchorDirection::Below);
        CHECK_EQ(p.rect.x, 1920 - 200); // 1720
        CHECK_EQ(p.rect.right(), 1920);
        CHECK(p.clamped_x);
        CHECK(!p.clamped_y);
    }

    // 4. Cursor near left edge (negative or overflowing left)
    {
        AnchorRect cursor{-10, 400, 2, 20};
        PopupPlacement p = CandidateManager::compute_placement(cursor, popup_w, popup_h, display, margin);
        CHECK_EQ(p.rect.x, 0);
        CHECK(p.clamped_x);
    }

    // 5. Very small display where neither side fits: clamped y
    {
        AnchorRect small_disp{0, 0, 500, 200};
        AnchorRect cursor{100, 90, 2, 20}; // space above = 90 - 5 = 85; space below = 200 - 115 = 85
        PopupPlacement p = CandidateManager::compute_placement(cursor, popup_w, popup_h, small_disp, margin);
        CHECK(p.clamped_y);
        CHECK_GE(p.rect.top(), small_disp.top());
        CHECK_LE(p.rect.bottom(), small_disp.bottom());
    }

    // 6. Secondary monitor offset (e.g. x = 1920, y = 100)
    {
        AnchorRect sec_disp{1920, 100, 1920, 1080};
        AnchorRect cursor{2100, 300, 2, 20};
        PopupPlacement p = CandidateManager::compute_placement(cursor, popup_w, popup_h, sec_disp, margin);
        CHECK_EQ(p.direction, AnchorDirection::Below);
        CHECK_EQ(p.rect.x, 2100);
        CHECK_EQ(p.rect.y, 325);
        CHECK(!p.clamped_x);
        CHECK(!p.clamped_y);

        // Clamping on secondary monitor right edge
        AnchorRect cursor_far{3800, 300, 2, 20};
        PopupPlacement p2 = CandidateManager::compute_placement(cursor_far, popup_w, popup_h, sec_disp, margin);
        CHECK_EQ(p2.rect.right(), 3840);
        CHECK(p2.clamped_x);
    }

    return bstest::finish("test_anchor_geometry");
}
