#ifndef PVZ_LAWN_WIDGET_VS_ACTION_AI_DRAFT_POLICY_H
#define PVZ_LAWN_WIDGET_VS_ACTION_AI_DRAFT_POLICY_H

#include "PvZ/Lawn/Common/ConstEnums.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace vsai::draft {

enum class BanDatabaseLoadState : std::uint8_t {
    Uninitialized,
    Unavailable,
    Invalid,
    Loaded,
};

// Replay Ban data is advisory. Callers retain their own baseline and matchup
// scores, while this loader only exposes a validated priority for one seed.
int BanDatabasePriority(bool targetsZombies, SeedType seed, std::uint32_t tick);
BanDatabaseLoadState GetBanDatabaseLoadState();
void ResetBanDatabase();

bool IsPlantTempoMushroom(SeedType seed);
bool IsPlantCarrySeed(SeedType seed);
bool IsPeaMainDamageSeed(SeedType seed);
bool IsCoffeeDependentPlant(SeedType seed);
bool IsMagnetTargetZombieSeed(SeedType seed);

template <typename IsEligible>
SeedType FindRotatedEligibleSeed(std::span<const SeedType> seeds, std::size_t firstIndex, IsEligible &&isEligible) {
    if (seeds.empty()) {
        return SeedType::SEED_NONE;
    }
    firstIndex %= seeds.size();
    for (std::size_t offset = 0; offset < seeds.size(); ++offset) {
        const SeedType seed = seeds[(firstIndex + offset) % seeds.size()];
        if (isEligible(seed)) {
            return seed;
        }
    }
    return SeedType::SEED_NONE;
}

} // namespace vsai::draft

#endif // PVZ_LAWN_WIDGET_VS_ACTION_AI_DRAFT_POLICY_H
