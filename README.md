# broime

Standalone C++20 compose key, dead key, and input method candidate engine for the bro desktop ecosystem.

`broime` provides deterministic compose sequence processing (XCompose format, Multi_key, dead keys, accent combinations), a paged candidate selection model, cursor-anchored popup placement geometry, and a compact binary prefix trie with `brosearch` fuzzy matching integration.

---

## Features

- **Compose Sequence Tree & Dead Keys (`ComposeEngine`)**:
  - Deterministic sequence trie for compose keys and dead keys.
  - Standard XCompose parser handling `<Multi_key>`, `<dead_*>` symbols, comments, whitespace, hex codes (`\xHH`), and string escape sequences.
  - Built-in comprehensive rules for dead keys:
    - Dead acute (`' + e -> é`, `DeadAcute + e -> é`)
    - Dead grave (`` ` + a -> à ``)
    - Dead circumflex (`^ + o -> ô`)
    - Dead tilde (`~ + a -> ã`)
    - Dead diaeresis / umlaut (`" + u -> ü`)
    - Dead cedilla (`, + c -> ç`)
    - Dead caron (`c -> č`, `s -> š`, `z -> ž`)
    - Dead abovering (`a -> å`, `u -> ů`)
    - Dead macron, ogonek, stroke, double acute
  - Common Multi_key compose symbols: `©`, `®`, `™`, `ß`, `æ`, `œ`, `¢`, `¥`, `€`, `£`, `«`, `»`, `→`, `←`, `⇒`, `…`, `–`, `—`, `×`, `÷`, `½`, `°`, `±`, `µ`, `§`, `¶`.
  - Configurable ASCII dead-key accent fallback toggle.

- **Paged Candidate Model (`CandidateManager`)**:
  - Paged candidate management with configurable page size (default 9 for 1..9 number hotkeys).
  - Wrap-around navigation: `select_next()`, `select_prev()`, `next_page()`, `prev_page()`.
  - Direct selection via global index or page-relative index (`select_page_index()`).
  - Fuzzy candidate filtering powered by `brosearch::fuzzy_filter`.

- **Cursor-Anchor Placement Geometry**:
  - `compute_placement()` calculates optimal popup window bounding boxes relative to active cursor/insertion point rectangles.
  - Prioritizes placing below cursor; automatically flips above cursor when display bottom edge would overflow.
  - Gracefully clamps horizontally and vertically to remain fully visible within display boundaries.
  - Supports multi-monitor setups with arbitrary display offset rectangles.

- **Compact Binary Dictionary Trie (`DictionaryTrie`)**:
  - In-memory prefix trie for candidate word/token suggestions.
  - Scoring & ranking: results sorted by score (descending), length (ascending), and lexicographical order.
  - Binary serialization format: compact portable binary serialization (`serialize()`, `deserialize()`) with magic header (`"BIME"`), versioning, and node counts.
  - File I/O: `save_to_file()` and `load_from_file()`.
  - Fuzzy matching fallback via `brosearch::FuzzyQuery`.

- **Unified IME Engine (`ImeEngine`)**:
  - Coordinates compose sequences, preedit buffer editing, candidate paging, and dictionary completions.
  - Handles Backspace, Space (commit selected), Return (commit raw preedit), Escape (cancel), and 1..9 selection hotkeys.
  - Configurable operating modes: `InputMode::Compose`, `InputMode::Dictionary`, `InputMode::Bypass`.

---

## Repository Layout

```
broime/
├── CMakeLists.txt              # Top-level build configuration (Ninja job pools, brosearch)
├── include/
│   └── broime/
│       ├── broime.h            # Umbrella include
│       ├── ime.h               # ImeEngine, ComposeEngine, CandidateManager, DictionaryTrie
│       └── types.h             # ComposeSequence, ComposeResult, CandidateEntry, AnchorRect, CandidatePage
├── src/
│   ├── candidate_model.cpp     # CandidateManager, pagination, placement geometry
│   ├── compose_table.cpp       # Deterministic sequence tree, XCompose parser, builtin tables
│   ├── dictionary_trie.cpp     # Prefix trie, binary serialization, fuzzy matching
│   ├── ime_engine.cpp          # ImeEngine coordinator and preedit buffer
│   └── types.cpp               # Keysym tables, string conversions, ComposeSequence
├── tests/
│   ├── CMakeLists.txt          # Test targets registration
│   ├── check.h                 # Clean test assertion framework (no assert())
│   ├── test_anchor_geometry.cpp# Placement geometry & boundary clamping tests
│   ├── test_candidate_model.cpp# Candidate pagination & selection tests
│   ├── test_compose.cpp        # Dead keys & compose sequences tests
│   ├── test_dictionary_trie.cpp# Trie prefix, ranking, binary round-trip tests
│   ├── test_ime_engine.cpp     # End-to-end IME workflow integration tests
│   ├── test_types.cpp          # Types, keysyms, and sequences tests
│   └── test_xcompose_parser.cpp# XCompose file/string parser tests
├── LICENSE                     # MIT License
└── README.md
```

All source files are strictly decomposed and kept under 500 lines.

---

## Building and Testing

### Prerequisites
- CMake 3.24+
- C++20 compliant compiler (GCC 12+, Clang 15+, MSVC 2022)
- Ninja
- Nothing else to check out: `brosearch` and, for the JavaScript binding, bronze are
  `bro_dependency()` pins in `CMakeLists.txt` (`cmake/bro_deps.cmake`), taken from a working
  tree at `../<name>` when there is one and otherwise fetched at configure (override with
  `-DFETCHCONTENT_SOURCE_DIR_<NAME>=<path>`)

### Build
```bash
cmake -B build -S . -GNinja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

### Run Tests
```bash
ctest --test-dir build --output-on-failure
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
