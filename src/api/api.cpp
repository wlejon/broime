#include "api.h"
#include "host_ime_internal.h"

#include <mutex>

namespace bro::ime::api {

namespace {

std::mutex g_services_mu;

std::shared_ptr<bro::ime::ComposeEngine> g_custom_compose_engine;
std::shared_ptr<bro::ime::ComposeEngine> g_default_compose_engine;

std::shared_ptr<bro::ime::CandidateManager> g_custom_candidate_manager;
std::shared_ptr<bro::ime::CandidateManager> g_default_candidate_manager;

std::shared_ptr<bro::ime::DictionaryTrie> g_custom_dictionary_trie;
std::shared_ptr<bro::ime::DictionaryTrie> g_default_dictionary_trie;

} // namespace

std::shared_ptr<bro::ime::ComposeEngine> activeComposeEngine() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_compose_engine) return g_custom_compose_engine;
    if (!g_default_compose_engine) {
        g_default_compose_engine = std::make_shared<bro::ime::ComposeEngine>();
    }
    return g_default_compose_engine;
}

void setComposeEngine(std::shared_ptr<bro::ime::ComposeEngine> engine) {
    std::lock_guard lock(g_services_mu);
    g_custom_compose_engine = std::move(engine);
}

std::shared_ptr<bro::ime::ComposeEngine> getComposeEngine() {
    return activeComposeEngine();
}

std::shared_ptr<bro::ime::CandidateManager> activeCandidateManager() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_candidate_manager) return g_custom_candidate_manager;
    if (!g_default_candidate_manager) {
        g_default_candidate_manager = std::make_shared<bro::ime::CandidateManager>();
    }
    return g_default_candidate_manager;
}

void setCandidateManager(std::shared_ptr<bro::ime::CandidateManager> mgr) {
    std::lock_guard lock(g_services_mu);
    g_custom_candidate_manager = std::move(mgr);
}

std::shared_ptr<bro::ime::CandidateManager> getCandidateManager() {
    return activeCandidateManager();
}

std::shared_ptr<bro::ime::DictionaryTrie> activeDictionaryTrie() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_dictionary_trie) return g_custom_dictionary_trie;
    if (!g_default_dictionary_trie) {
        g_default_dictionary_trie = std::make_shared<bro::ime::DictionaryTrie>();
    }
    return g_default_dictionary_trie;
}

void setDictionaryTrie(std::shared_ptr<bro::ime::DictionaryTrie> trie) {
    std::lock_guard lock(g_services_mu);
    g_custom_dictionary_trie = std::move(trie);
}

std::shared_ptr<bro::ime::DictionaryTrie> getDictionaryTrie() {
    return activeDictionaryTrie();
}

Value ensureBroIme() {
    ev::Persistent globalThisVal;
    auto gt = ev::globalValue("globalThis");
    if (gt.found && ev::isObject(gt.value)) {
        globalThisVal.set(gt.value);
    }

    ev::Persistent broP;
    auto bro = ev::globalValue("bro");
    if (bro.found && ev::isObject(bro.value)) broP.set(bro.value);
    if (!ev::isObject(broP.get()) && ev::isObject(globalThisVal.get())) {
        Value candidate = ev::getProperty(globalThisVal.get(), "bro");
        if (ev::isObject(candidate)) broP.set(candidate);
    }
    if (!ev::isObject(broP.get())) {
        broP.set(ev::createObject());
        ev::registerGlobal("bro", broP.get());
        if (ev::isObject(globalThisVal.get())) {
            globalThisVal.set(ev::setProperty(globalThisVal.get(), "bro", broP.get()));
        }
    }

    ev::Persistent imeP(ev::getProperty(broP.get(), "ime"));
    if (!ev::isObject(imeP.get())) {
        imeP.set(ev::createObject());
        broP.set(ev::setProperty(broP.get(), "ime", imeP.get()));
    }
    return imeP.get();
}

void installIme() {
    ev::Persistent imeObj(ensureBroIme());
    installNativeImeOnto(imeObj.get());
}

void tickImeAsync() {
    ev::drainMicrotasks();
}

void shutdownImeAsync() {
    auto compose = activeComposeEngine();
    if (compose) {
        compose->reset();
    }
    auto cand = activeCandidateManager();
    if (cand) {
        cand->clear();
    }
}

} // namespace bro::ime::api
