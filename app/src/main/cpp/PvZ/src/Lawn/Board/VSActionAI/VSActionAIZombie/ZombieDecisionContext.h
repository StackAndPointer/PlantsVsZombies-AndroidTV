#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_DECISION_CONTEXT_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_DECISION_CONTEXT_H

#include "ZombieCardRules.h"

namespace vsai::detail {

struct ZombieDecisionContext {
    ZombieTempoPolicy tempo;
    ZombieTemplateProfile templateProfile;
    int actualEconomyCount = 0;
    int economyCount = 0;
    int economyTarget = 0;
    int economyDeficit = 0;
    int activePressureRows = 0;
    int heavyEconomyThreshold = 0;
};

ZombieDecisionContext BuildZombieDecisionContext(const VSGameState &state);

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_DECISION_CONTEXT_H
