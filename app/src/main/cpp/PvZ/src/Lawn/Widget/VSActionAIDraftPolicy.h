#ifndef PVZ_LAWN_WIDGET_VS_ACTION_AI_DRAFT_POLICY_H
#define PVZ_LAWN_WIDGET_VS_ACTION_AI_DRAFT_POLICY_H

#include "PvZ/Lawn/Common/ConstEnums.h"

#include <cstddef>
#include <span>

namespace vsai::draft {

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
