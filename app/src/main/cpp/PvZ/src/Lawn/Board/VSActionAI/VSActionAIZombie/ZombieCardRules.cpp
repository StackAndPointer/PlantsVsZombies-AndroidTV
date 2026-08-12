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

bool IsZombieMetalGraveGuard(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_PAIL || seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR
        || seed == SeedType::SEED_ZOMBIE_TRASHCAN;
}

bool IsZombieLobbedScreenDonation(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_TRASHCAN || seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR
        || seed == SeedType::SEED_ZOMBIE_NEWSPAPER;
}

int ZombieGraveGuardPriority(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_TRASHCAN:
            return 520;
        case SeedType::SEED_ZOMBIE_TALLNUT_HEAD:
            return 465;
        case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
            return 410;
        case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
            return 380;
        case SeedType::SEED_ZOMBIE_PAIL:
            return 340;
        case SeedType::SEED_ZOMBIE_SUNDAY_EDITION:
            return 285;
        case SeedType::SEED_ZOMBIE_NEWSPAPER:
            return 255;
        case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
            return 160;
        default:
            return 0;
    }
}

} // namespace vsai::detail
