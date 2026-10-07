#include "host_ime_internal.h"
#include "arg_reader.h"
#include "object_builder.h"

#include <broime/ime.h>
#include <broime/types.h>

namespace bro::ime::api {

namespace {

AnchorRect readAnchorRect(Value v) {
    AnchorRect r;
    if (!ev::isObject(v)) return r;
    ev::Persistent obj(v);

    auto getNum = [&](std::string_view prop, double def = 0.0) -> double {
        Value val = ev::getProperty(obj.get(), prop);
        if (ev::isNumber(val)) return ev::toDouble(val);
        return def;
    };

    double x = getNum("x", getNum("left", 0.0));
    double y = getNum("y", getNum("top", 0.0));
    double w = getNum("width", getNum("w", 0.0));
    double h = getNum("height", getNum("h", 0.0));

    r.x = static_cast<int>(x);
    r.y = static_cast<int>(y);
    r.width = static_cast<int>(w);
    r.height = static_cast<int>(h);
    return r;
}

} // namespace

void installNativeImeOnto(Value imeObj) {
    ObjectBuilder ime(imeObj);

    // bro.ime.feedKey(keyNameOrSym) -> { status: 'none'|'pending'|'matched', text: string }
    ime.def("feedKey", 1, [](Value, std::span<const Value> args) -> Value {
        KeySym sym = keysyms::VoidSymbol;
        if (!args.empty()) {
            if (ev::isNumber(args[0])) {
                sym = static_cast<KeySym>(ev::toDouble(args[0]));
            } else if (ev::isString(args[0])) {
                std::string s = ev::toUtf8(args[0]);
                sym = keysym_from_name(s);
                if (sym == keysyms::VoidSymbol && !s.empty()) {
                    sym = keysym_from_utf8(s);
                }
            }
        }

        if (sym == keysyms::VoidSymbol) {
            ObjectBuilder b;
            b.set("status", "none");
            b.set("text", "");
            return b.build();
        }

        auto compose = activeComposeEngine();
        ComposeResult res = compose ? compose->feed(sym) : ComposeResult(ComposeResult::None);

        ObjectBuilder b;
        std::string statusStr = "none";
        if (res.status == ComposeResult::Pending) {
            statusStr = "pending";
        } else if (res.status == ComposeResult::Matched) {
            statusStr = "matched";
        }
        b.set("status", statusStr);
        b.set("text", res.text);
        return b.build();
    });

    // bro.ime.reset() -> void
    ime.def("reset", 0, [](Value, std::span<const Value>) -> Value {
        auto compose = activeComposeEngine();
        if (compose) {
            compose->reset();
        }
        return ev::undefined();
    });

    // bro.ime.isComposing() -> boolean
    ime.def("isComposing", 0, [](Value, std::span<const Value>) -> Value {
        auto compose = activeComposeEngine();
        bool composing = compose ? compose->is_composing() : false;
        return ev::fromBool(composing);
    });

    // bro.ime.keysymFromName(name) -> number
    ime.def("keysymFromName", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) {
            return ev::fromDouble(keysyms::VoidSymbol);
        }
        std::string name = ev::toUtf8(args[0]);
        KeySym sym = keysym_from_name(name);
        if (sym == keysyms::VoidSymbol && !name.empty()) {
            sym = keysym_from_utf8(name);
        }
        return ev::fromDouble(static_cast<double>(sym));
    });

    // bro.ime.keysymToName(sym) -> string
    ime.def("keysymToName", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isNumber(args[0])) {
            return ev::fromUtf8("");
        }
        KeySym sym = static_cast<KeySym>(ev::toDouble(args[0]));
        std::string name = keysym_to_name(sym);
        return ev::fromUtf8(name);
    });

    // bro.ime.keysymToUtf8(sym) -> string
    ime.def("keysymToUtf8", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isNumber(args[0])) {
            return ev::fromUtf8("");
        }
        KeySym sym = static_cast<KeySym>(ev::toDouble(args[0]));
        std::string utf8 = keysym_to_utf8(sym);
        return ev::fromUtf8(utf8);
    });

    // bro.ime.keysymFromUtf8(str) -> number
    ime.def("keysymFromUtf8", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty() || !ev::isString(args[0])) {
            return ev::fromDouble(keysyms::VoidSymbol);
        }
        std::string str = ev::toUtf8(args[0]);
        KeySym sym = keysym_from_utf8(str);
        return ev::fromDouble(static_cast<double>(sym));
    });

    // bro.ime.computePopupPlacement(cursorRect, popupWidth, popupHeight, displayBounds, [margin])
    // -> { x, y, width, height, flippedY }
    ime.def("computePopupPlacement", 4, [](Value, std::span<const Value> args) -> Value {
        AnchorRect cursor_rect = readAnchorRect(args.size() > 0 ? args[0] : ev::undefined());
        int popup_w = args.size() > 1 && ev::isNumber(args[1]) ? static_cast<int>(ev::toDouble(args[1])) : 0;
        int popup_h = args.size() > 2 && ev::isNumber(args[2]) ? static_cast<int>(ev::toDouble(args[2])) : 0;
        AnchorRect display_bounds = readAnchorRect(args.size() > 3 ? args[3] : ev::undefined());
        int margin = args.size() > 4 && ev::isNumber(args[4]) ? static_cast<int>(ev::toDouble(args[4])) : 4;

        PopupPlacement placement = CandidateManager::compute_placement(
            cursor_rect, popup_w, popup_h, display_bounds, margin
        );

        ObjectBuilder b;
        b.set("x", static_cast<double>(placement.rect.x));
        b.set("y", static_cast<double>(placement.rect.y));
        b.set("width", static_cast<double>(placement.rect.width));
        b.set("height", static_cast<double>(placement.rect.height));
        b.set("flippedY", placement.direction == AnchorDirection::Above);
        return b.build();
    });

    // bro.ime.findPrefixSuggestions(prefix, [maxResults]) -> string[]
    ime.def("findPrefixSuggestions", 1, [](Value, std::span<const Value> args) -> Value {
        std::string prefix = (args.size() > 0 && ev::isString(args[0])) ? ev::toUtf8(args[0]) : "";
        size_t max_results = 20;
        if (args.size() > 1 && ev::isNumber(args[1])) {
            double d = ev::toDouble(args[1]);
            if (d >= 0) max_results = static_cast<size_t>(d);
        }

        auto trie = activeDictionaryTrie();
        std::vector<CandidateEntry> entries;
        if (trie) {
            entries = trie->lookup_prefix(prefix, max_results);
        }

        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(entries.size())));
        for (uint32_t i = 0; i < entries.size(); ++i) {
            ev::Persistent s(ev::fromUtf8(entries[i].text));
            arr.set(ev::setElement(arr.get(), i, s.get()));
        }
        return arr.get();
    });

    // bro.ime.insertWord(word, score, annotation) -> boolean
    ime.def("insertWord", 2, [](Value, std::span<const Value> args) -> Value {
        std::string word = (args.size() > 0 && ev::isString(args[0])) ? ev::toUtf8(args[0]) : "";
        int32_t score = (args.size() > 1 && ev::isNumber(args[1])) ? static_cast<int32_t>(ev::toDouble(args[1])) : 0;
        std::string annot = (args.size() > 2 && ev::isString(args[2])) ? ev::toUtf8(args[2]) : "";
        auto trie = activeDictionaryTrie();
        if (trie && !word.empty()) {
            trie->insert(word, score, annot);
            return ev::fromBool(true);
        }
        return ev::fromBool(false);
    });

    // bro.ime.addRule(spec, output) -> boolean
    ime.def("addRule", 2, [](Value, std::span<const Value> args) -> Value {
        std::string spec = (args.size() > 0 && ev::isString(args[0])) ? ev::toUtf8(args[0]) : "";
        std::string out = (args.size() > 1 && ev::isString(args[1])) ? ev::toUtf8(args[1]) : "";
        auto compose = activeComposeEngine();
        if (compose && !spec.empty() && !out.empty()) {
            compose->add_rule(spec, out);
            return ev::fromBool(true);
        }
        return ev::fromBool(false);
    });

    // bro.ime.clearRules() -> void
    ime.def("clearRules", 0, [](Value, std::span<const Value>) -> Value {
        auto compose = activeComposeEngine();
        if (compose) {
            compose->clear_rules();
        }
        return ev::undefined();
    });
}

} // namespace bro::ime::api
