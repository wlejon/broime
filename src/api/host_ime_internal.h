#pragma once

#include "embed/embed.h"
#include <broime/ime.h>
#include <broime/types.h>

#include <memory>

namespace bro::ime::api {

namespace ev = bronze::embed;
using Value = bronze::Value;

Value ensureBroIme();

void installNativeImeOnto(Value imeObj);

std::shared_ptr<bro::ime::ComposeEngine> activeComposeEngine();
std::shared_ptr<bro::ime::CandidateManager> activeCandidateManager();
std::shared_ptr<bro::ime::DictionaryTrie> activeDictionaryTrie();

} // namespace bro::ime::api
