#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_CARD_RULES_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_CARD_RULES_H

#include "../VSActionAIStrategy.h"

namespace vsai::detail {

bool IsZombieTargetedSeed(SeedType seed);
bool IsZombieEconomySeed(SeedType seed);
bool IsZombieFrontlineProbeSeed(SeedType seed);
bool IsZombieFastAttackSeed(SeedType seed);

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_CARD_RULES_H
