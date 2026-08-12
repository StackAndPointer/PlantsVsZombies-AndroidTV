#include "ZombieCardRules.h"

namespace vsai::detail {

bool IsZombieTargetedSeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_BUNGEE;
}

bool IsZombieEconomySeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_GRAVESTONE || seed == SeedType::SEED_ZOMBIE_MOUND;
}

bool IsZombieFrontlineProbeSeed(SeedType seed) {
    if (IsZombieEconomySeed(seed) || IsZombieTargetedSeed(seed) || IsHeavyZombieSeed(seed)) {
        return false;
    }
    return seed != SeedType::SEED_ZOMBIE_TRASHCAN && seed != SeedType::SEED_ZOMBIE_WALLNUT_HEAD
        && seed != SeedType::SEED_ZOMBIE_TALLNUT_HEAD;
}

bool IsZombieFastAttackSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_NORMAL:
        case SeedType::SEED_ZOMBIE_IMP:
        case SeedType::SEED_ZOMBIE_SUPER_FAN_IMP:
        case SeedType::SEED_ZOMBIE_DOGWALKER:
        case SeedType::SEED_ZOMBIE_FLAG:
        case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
            return true;
        default:
            return false;
    }
}

} // namespace vsai::detail
