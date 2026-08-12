#include "ZombieDecisionContext.h"

#include <algorithm>

namespace vsai::detail {

ZombieDecisionContext BuildZombieDecisionContext(const VSGameState &state) {
    ZombieDecisionContext context{.tempo = GetZombieTempoPolicy()};
    context.actualEconomyCount = CountZombieEconomy(state);
    context.economyCount = context.tempo.EffectiveEconomyCount(context.actualEconomyCount);
    context.economyTarget = state.isSuddenDeath ? context.economyCount
        : context.tempo.EconomyTarget(std::max(state.rows * 2, state.rows * 3), state.rows);
    context.economyDeficit = std::max(0, context.economyTarget - context.economyCount);
    context.activePressureRows = CountActiveZombieRows(state);
    context.heavyEconomyThreshold = HeavyZombieEconomyThreshold(state);
    context.templateProfile = DetectZombieTemplateProfile(state);
    return context;
}

} // namespace vsai::detail
