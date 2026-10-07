#pragma once

#include <broime/ime.h>
#include <broime/types.h>

#include <memory>

namespace bro::ime::api {

/// Mounts `bro.ime` in the current Bronze realm.
void installIme();

/// Pumps async IME microtasks.
void tickImeAsync();

/// Cleans up active IME resources.
void shutdownImeAsync();

/// Sets the ComposeEngine used by the API (defaults to std::make_shared<ComposeEngine>()).
void setComposeEngine(std::shared_ptr<bro::ime::ComposeEngine> engine);

/// Gets the ComposeEngine currently used by the API.
std::shared_ptr<bro::ime::ComposeEngine> getComposeEngine();

/// Sets the CandidateManager used by the API.
void setCandidateManager(std::shared_ptr<bro::ime::CandidateManager> mgr);

/// Gets the CandidateManager currently used by the API.
std::shared_ptr<bro::ime::CandidateManager> getCandidateManager();

/// Sets the DictionaryTrie used by the API.
void setDictionaryTrie(std::shared_ptr<bro::ime::DictionaryTrie> trie);

/// Gets the DictionaryTrie currently used by the API.
std::shared_ptr<bro::ime::DictionaryTrie> getDictionaryTrie();

} // namespace bro::ime::api

using bro::ime::api::installIme;
using bro::ime::api::tickImeAsync;
using bro::ime::api::shutdownImeAsync;
using bro::ime::api::setComposeEngine;
using bro::ime::api::getComposeEngine;
using bro::ime::api::setCandidateManager;
using bro::ime::api::getCandidateManager;
using bro::ime::api::setDictionaryTrie;
using bro::ime::api::getDictionaryTrie;
