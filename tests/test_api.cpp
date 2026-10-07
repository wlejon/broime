#include "../src/api/api.h"
#include "embed/embed.h"
#include "eval/eval.h"
#include <broime/ime.h>
#include <broime/types.h>

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond " (line " << __LINE__ << ")" \
                      << std::endl;                                        \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

int main() {
    namespace ev = bronze::embed;
    using namespace bronze::eval;

    std::cout << "Starting broime JavaScript API test..." << std::endl;

    // 1. Install bro.ime into Bronze realm
    bro::ime::api::installIme();

    auto g = ev::globalValue("bro");
    CHECK(g.found);
    CHECK(ev::isObject(g.value));

    ev::Persistent ime(ev::getProperty(g.value, "ime"));
    CHECK(ev::isObject(ime.get()));
    std::cout << "  Mounted bro.ime successfully." << std::endl;

    // Verify all required methods exist
    const char* methods[] = {
        "feedKey", "reset", "isComposing",
        "keysymFromName", "keysymToName", "keysymToUtf8", "keysymFromUtf8",
        "computePopupPlacement", "findPrefixSuggestions"
    };
    for (const char* m : methods) {
        auto fn = ev::getProperty(ime.get(), m);
        CHECK(ev::isFunction(fn));
        std::cout << "  Found bro.ime." << m << std::endl;
    }

    // 2. Test feedKey('DeadAcute') + feedKey('e') -> matches 'é'
    std::cout << "Testing feedKey('DeadAcute') + feedKey('e')..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  bro.ime.reset();\n"
            "  if (bro.ime.isComposing() !== false) return false;\n"
            "  const r1 = bro.ime.feedKey('DeadAcute');\n"
            "  if (typeof r1 !== 'object' || r1 === null) return false;\n"
            "  if (r1.status !== 'pending') return false;\n"
            "  if (r1.text !== '') return false;\n"
            "  if (bro.ime.isComposing() !== true) return false;\n"
            "\n"
            "  const r2 = bro.ime.feedKey('e');\n"
            "  if (typeof r2 !== 'object' || r2 === null) return false;\n"
            "  if (r2.status !== 'matched') return false;\n"
            "  if (r2.text !== 'é') return false;\n"
            "  if (bro.ime.isComposing() !== false) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  feedKey('DeadAcute') + feedKey('e') -> 'é' [PASS]" << std::endl;
    }

    // 3. Test feedKey with numeric KeySym (e.g. 0xFE50 for DeadGrave + 'a' -> 'à')
    std::cout << "Testing feedKey with numeric keysym..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  bro.ime.reset();\n"
            "  const r1 = bro.ime.feedKey(0xFE50);\n"
            "  if (r1.status !== 'pending') return false;\n"
            "  const r2 = bro.ime.feedKey('a');\n"
            "  if (r2.status !== 'matched' || r2.text !== 'à') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  feedKey(numeric) [PASS]" << std::endl;
    }

    // 4. Test reset() and isComposing()
    std::cout << "Testing reset() and isComposing()..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  bro.ime.feedKey('DeadAcute');\n"
            "  if (!bro.ime.isComposing()) return false;\n"
            "  bro.ime.reset();\n"
            "  if (bro.ime.isComposing()) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  reset() and isComposing() [PASS]" << std::endl;
    }

    // 5. Test invalid sequence / cancellation
    std::cout << "Testing invalid sequence handling..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  bro.ime.reset();\n"
            "  bro.ime.feedKey('DeadAcute');\n"
            "  const r_inv = bro.ime.feedKey('9');\n"
            "  if (r_inv.status !== 'none') return false;\n"
            "  if (bro.ime.isComposing()) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  invalid sequence handling [PASS]" << std::endl;
    }

    // 6. Test keysym conversions: keysymFromName, keysymToName, keysymToUtf8, keysymFromUtf8
    std::cout << "Testing keysym conversion functions..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const symAcute = bro.ime.keysymFromName('DeadAcute');\n"
            "  if (symAcute !== 0xFE51) return false;\n"
            "  const symAcuteLower = bro.ime.keysymFromName('dead_acute');\n"
            "  if (symAcuteLower !== 0xFE51) return false;\n"
            "  const symSpace = bro.ime.keysymFromName('space');\n"
            "  if (symSpace !== 0x0020) return false;\n"
            "  const symE = bro.ime.keysymFromName('e');\n"
            "  if (symE !== 101) return false;\n"
            "\n"
            "  const nameAcute = bro.ime.keysymToName(0xFE51);\n"
            "  if (nameAcute !== 'dead_acute') return false;\n"
            "  const nameE = bro.ime.keysymToName(101);\n"
            "  if (nameE !== 'e') return false;\n"
            "\n"
            "  const utf8E = bro.ime.keysymToUtf8(101);\n"
            "  if (utf8E !== 'e') return false;\n"
            "  const utf8Acute = bro.ime.keysymToUtf8(0x00E9);\n"
            "  if (utf8Acute !== 'é') return false;\n"
            "\n"
            "  const symFromUtf8E = bro.ime.keysymFromUtf8('e');\n"
            "  if (symFromUtf8E !== 101) return false;\n"
            "  const symFromUtf8Acute = bro.ime.keysymFromUtf8('é');\n"
            "  if (symFromUtf8Acute !== 0x00E9) return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  keysym conversion functions [PASS]" << std::endl;
    }

    // 7. Test computePopupPlacement
    std::cout << "Testing computePopupPlacement()..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  // Normal cursor: fits below\n"
            "  const p1 = bro.ime.computePopupPlacement(\n"
            "    { x: 500, y: 400, width: 2, height: 20 },\n"
            "    200, 150,\n"
            "    { x: 0, y: 0, width: 1920, height: 1080 }\n"
            "  );\n"
            "  if (typeof p1 !== 'object' || p1 === null) return false;\n"
            "  if (p1.x !== 500) return false;\n"
            "  if (p1.y !== 424) return false; // 400 + 20 + 4 margin\n"
            "  if (p1.width !== 200 || p1.height !== 150) return false;\n"
            "  if (p1.flippedY !== false) return false;\n"
            "\n"
            "  // Bottom cursor: flips above\n"
            "  const p2 = bro.ime.computePopupPlacement(\n"
            "    { x: 500, y: 1000, width: 2, height: 20 },\n"
            "    200, 150,\n"
            "    { x: 0, y: 0, width: 1920, height: 1080 }\n"
            "  );\n"
            "  if (p2.x !== 500) return false;\n"
            "  if (p2.y !== 846) return false; // 1000 - 4 - 150\n"
            "  if (p2.flippedY !== true) return false;\n"
            "\n"
            "  // Right edge cursor: clamps x\n"
            "  const p3 = bro.ime.computePopupPlacement(\n"
            "    { x: 1850, y: 400, width: 2, height: 20 },\n"
            "    200, 150,\n"
            "    { x: 0, y: 0, width: 1920, height: 1080 }\n"
            "  );\n"
            "  if (p3.x !== 1720) return false; // 1920 - 200\n"
            "  if (p3.width !== 200) return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  computePopupPlacement() [PASS]" << std::endl;
    }

    // 8. Test findPrefixSuggestions
    std::cout << "Testing findPrefixSuggestions()..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  bro.ime.insertWord('hello', 100, 'greeting');\n"
            "  bro.ime.insertWord('help', 80, 'support');\n"
            "  bro.ime.insertWord('helmet', 60, 'gear');\n"
            "  bro.ime.insertWord('world', 90, 'noun');\n"
            "\n"
            "  const res = bro.ime.findPrefixSuggestions('hel', 10);\n"
            "  if (!Array.isArray(res)) return false;\n"
            "  if (res.length !== 3) return false;\n"
            "  if (res[0] !== 'hello') return false;\n"
            "  if (res[1] !== 'help') return false;\n"
            "  if (res[2] !== 'helmet') return false;\n"
            "\n"
            "  // Max results limit\n"
            "  const res2 = bro.ime.findPrefixSuggestions('hel', 2);\n"
            "  if (!Array.isArray(res2) || res2.length !== 2) return false;\n"
            "  if (res2[0] !== 'hello' || res2[1] !== 'help') return false;\n"
            "\n"
            "  // Nonexistent prefix\n"
            "  const res3 = bro.ime.findPrefixSuggestions('xyz', 5);\n"
            "  if (!Array.isArray(res3) || res3.length !== 0) return false;\n"
            "\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  findPrefixSuggestions() [PASS]" << std::endl;
    }

    // 9. Test C++ accessors
    std::cout << "Testing C++ accessors..." << std::endl;
    {
        auto engine = bro::ime::api::getComposeEngine();
        CHECK(engine != nullptr);
        auto customEngine = std::make_shared<bro::ime::ComposeEngine>();
        bro::ime::api::setComposeEngine(customEngine);
        CHECK(bro::ime::api::getComposeEngine() == customEngine);
        // Restore
        bro::ime::api::setComposeEngine(engine);

        auto candMgr = bro::ime::api::getCandidateManager();
        CHECK(candMgr != nullptr);

        auto trie = bro::ime::api::getDictionaryTrie();
        CHECK(trie != nullptr);
        std::cout << "  C++ accessors [PASS]" << std::endl;
    }

    // 10. Test tickImeAsync and shutdownImeAsync
    std::cout << "Testing tickImeAsync() and shutdownImeAsync()..." << std::endl;
    {
        bro::ime::api::tickImeAsync();
        bro::ime::api::shutdownImeAsync();
        std::cout << "  tickImeAsync() and shutdownImeAsync() [PASS]" << std::endl;
    }

    // 11. GC stress loop: ensure memory stability under moving GC
    std::cout << "Running GC stress simulation loop..." << std::endl;
    {
        auto rStress = evalScript(
            "(function() {\n"
            "  for (let i = 0; i < 200; ++i) {\n"
            "    bro.ime.feedKey('DeadAcute');\n"
            "    bro.ime.feedKey('e');\n"
            "    bro.ime.computePopupPlacement(\n"
            "      { x: 100, y: 100, width: 10, height: 10 },\n"
            "      50, 50,\n"
            "      { x: 0, y: 0, width: 1000, height: 1000 }\n"
            "    );\n"
            "    bro.ime.findPrefixSuggestions('hel', 5);\n"
            "  }\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rStress.thrown && ev::toBool(rStress.value));
        bro::ime::api::tickImeAsync();
        std::cout << "  GC stress loop [PASS]" << std::endl;
    }

    std::cout << "All broime JavaScript API tests PASSED!" << std::endl;
    return 0;
}
