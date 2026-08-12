#include "VSActionAIDraftPolicy.h"

namespace vsai::draft {

bool IsPlantTempoMushroom(SeedType seed) {
    return seed == SeedType::SEED_PUFFSHROOM;
}

bool IsPlantCarrySeed(SeedType seed) {
    if (IsPlantTempoMushroom(seed)) {
        return false;
    }
    switch (seed) {
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_BLOOMERANG:
        case SeedType::SEED_STARFRUIT:
        case SeedType::SEED_SCAREDYSHROOM:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_SPORESHROOM:
            return true;
        default:
            return false;
    }
}

bool IsPeaMainDamageSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_GATLINGPEA:
            return true;
        default:
            return false;
    }
}

bool IsCoffeeDependentPlant(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_PUFFSHROOM:
        case SeedType::SEED_SCAREDYSHROOM:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_GLOOMSHROOM:
        case SeedType::SEED_SPORESHROOM:
        case SeedType::SEED_HYPNOSHROOM:
        case SeedType::SEED_ICESHROOM:
        case SeedType::SEED_DOOMSHROOM:
        case SeedType::SEED_MAGNETSHROOM:
            return true;
        default:
            return false;
    }
}

bool IsMagnetTargetZombieSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_PAIL:
        case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
        case SeedType::SEED_ZOMBIE_FOOTBALL:
        case SeedType::SEED_ZOMBIE_JACK_IN_THE_BOX:
        case SeedType::SEED_ZOMBIE_DIGGER:
        case SeedType::SEED_ZOMBIE_POGO:
        case SeedType::SEED_ZOMBIE_LADDER:
        case SeedType::SEED_ZOMBIE_TRASHCAN:
            return true;
        default:
            return false;
    }
}

} // namespace vsai::draft
