/*
 * Copyright (C) 2023-2026 PvZ TV Touch Team
 *
 * This file is part of PlantsVsZombies-AndroidTV.
 *
 * PlantsVsZombies-AndroidTV is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 */

#include "PvZ/Lawn/VSActionAIDecision.h"

#include "PvZ/Lawn/Board/GridItem.h"
#include "PvZ/Lawn/Board/Plant.h"
#include "PvZ/Lawn/Common/ConstEnums.h"
#include "PvZ/SexyAppFramework/Buffer.h"
#include "PvZ/SexyAppFramework/SexyAppBase.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

namespace vsai {
namespace {

bool IsDeadOrOutside(const VSPlantState &plant) {
    return plant.dead || plant.position.row < 0 || plant.position.col < 0;
}

bool HasPlantAt(const VSGameState &state, VSGridPosition position) {
    return std::any_of(state.plants.begin(), state.plants.end(), [position](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.position.col == position.col && plant.position.row == position.row;
    });
}

bool HasPlantTypeAt(const VSGameState &state, SeedType seedType, VSGridPosition position) {
    return std::any_of(state.plants.begin(), state.plants.end(), [seedType, position](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.seedType == static_cast<std::uint16_t>(seedType) && plant.position.col == position.col
            && plant.position.row == position.row;
    });
}

bool HasGridItemAt(const VSGameState &state, VSGridPosition position) {
    return std::any_of(state.gridItems.begin(), state.gridItems.end(), [position](const VSGridItemState &item) {
        return !item.dead && item.position.col == position.col && item.position.row == position.row;
    });
}

const VSZombieState *FindClosestZombie(const VSGameState &state, int row = -1) {
    const VSZombieState *closest = nullptr;
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row < 0 || (row >= 0 && zombie.row != row)) {
            continue;
        }
        if (closest == nullptr || zombie.positionX < closest->positionX) {
            closest = &zombie;
        }
    }
    return closest;
}

int CountPlantsInRow(const VSGameState &state, int row) {
    return static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [row](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.position.row == row;
    }));
}

int CountZombiesInRow(const VSGameState &state, int row) {
    return static_cast<int>(std::count_if(state.zombies.begin(), state.zombies.end(), [row](const VSZombieState &zombie) {
        return !zombie.dead && zombie.row == row;
    }));
}

int CountActiveZombies(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.zombies.begin(), state.zombies.end(), [](const VSZombieState &zombie) {
        return !zombie.dead && zombie.row >= 0;
    }));
}

int CountActiveZombieRows(const VSGameState &state) {
    int count = 0;
    for (int row = 0; row < state.rows; ++row) {
        if (CountZombiesInRow(state, row) > 0) {
            ++count;
        }
    }
    return count;
}

int CountPlantType(const VSGameState &state, SeedType seedType) {
    return static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [seedType](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.seedType == static_cast<std::uint16_t>(seedType);
    }));
}

bool HasPlantTypeInRow(const VSGameState &state, SeedType seedType, int row) {
    return std::any_of(state.plants.begin(), state.plants.end(), [seedType, row](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.position.row == row && plant.seedType == static_cast<std::uint16_t>(seedType);
    });
}

bool IsHeavyZombie(std::uint16_t zombieType) {
    switch (static_cast<ZombieType>(zombieType)) {
        case ZombieType::ZOMBIE_PAIL:
        case ZombieType::ZOMBIE_FOOTBALL:
        case ZombieType::ZOMBIE_BOBSLED:
        case ZombieType::ZOMBIE_ZAMBONI:
        case ZombieType::ZOMBIE_GARGANTUAR:
        case ZombieType::ZOMBIE_WALLNUT_HEAD:
        case ZombieType::ZOMBIE_GIGA_FOOTBALL:
        case ZombieType::ZOMBIE_GIGA_POLEVAULTER:
        case ZombieType::ZOMBIE_GIGA_GARGANTUAR:
            return true;
        default:
            return false;
    }
}

bool IsFastZombie(std::uint16_t zombieType) {
    switch (static_cast<ZombieType>(zombieType)) {
        case ZombieType::ZOMBIE_BOBSLED:
        case ZombieType::ZOMBIE_ZAMBONI:
        case ZombieType::ZOMBIE_FOOTBALL:
        case ZombieType::ZOMBIE_GIGA_FOOTBALL:
        case ZombieType::ZOMBIE_GIGA_POLEVAULTER:
        case ZombieType::ZOMBIE_POLEVAULTER:
        case ZombieType::ZOMBIE_DIGGER:
        case ZombieType::ZOMBIE_IMP:
            return true;
        default:
            return false;
    }
}

bool IsDecisiveCounterZombie(std::uint16_t zombieType) {
    switch (static_cast<ZombieType>(zombieType)) {
        case ZombieType::ZOMBIE_BOBSLED:
        case ZombieType::ZOMBIE_ZAMBONI:
        case ZombieType::ZOMBIE_FOOTBALL:
        case ZombieType::ZOMBIE_POLEVAULTER:
        case ZombieType::ZOMBIE_GARGANTUAR:
        case ZombieType::ZOMBIE_GIGA_FOOTBALL:
        case ZombieType::ZOMBIE_GIGA_POLEVAULTER:
        case ZombieType::ZOMBIE_GIGA_GARGANTUAR:
            return true;
        default:
            return false;
    }
}

bool HasZombieTypeInRow(const VSGameState &state, int row, ZombieType zombieType) {
    return std::any_of(state.zombies.begin(), state.zombies.end(), [row, zombieType](const VSZombieState &zombie) {
        return !zombie.dead && zombie.row == row && zombie.zombieType == static_cast<std::uint16_t>(zombieType);
    });
}

int LargestZombieStackInRow(const VSGameState &state, int row) {
    constexpr float kGridCellWidth = 80.0f;
    int largestStack = 0;
    for (const VSZombieState &anchor : state.zombies) {
        if (anchor.dead || anchor.row != row) {
            continue;
        }

        int stackSize = 0;
        for (const VSZombieState &zombie : state.zombies) {
            if (zombie.dead || zombie.row != row) {
                continue;
            }
            const float distance = zombie.positionX - anchor.positionX;
            if (distance > -kGridCellWidth && distance < kGridCellWidth) {
                ++stackSize;
            }
        }
        largestStack = std::max(largestStack, stackSize);
    }
    return largestStack;
}

int LargestCherryBombClusterInRow(const VSGameState &state, int row) {
    constexpr float kCherryBombRadius = 115.0f;
    int largestCluster = 0;
    for (const VSZombieState &anchor : state.zombies) {
        if (anchor.dead || anchor.row != row) {
            continue;
        }

        int clusterSize = 0;
        for (const VSZombieState &zombie : state.zombies) {
            if (!zombie.dead && zombie.row == row && std::abs(zombie.positionX - anchor.positionX) <= kCherryBombRadius) {
                ++clusterSize;
            }
        }
        largestCluster = std::max(largestCluster, clusterSize);
    }
    return largestCluster;
}

int ZombieThreatWeight(std::uint16_t zombieType);

int CounterPressureScoreInRow(const VSGameState &state, int row) {
    int score = 0;
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        score += ZombieThreatWeight(zombie.zombieType);
        score += IsDecisiveCounterZombie(zombie.zombieType) ? 150 : 0;
        score += zombie.eating ? 90 : 0;
        score += std::clamp((880 - static_cast<int>(zombie.positionX)) / 8, 0, 70);
        if (zombie.bodyMaxHealth > 0 && zombie.bodyHealth * 100 / zombie.bodyMaxHealth >= 70) {
            score += IsHeavyZombie(zombie.zombieType) ? 45 : 0;
        }
    }
    // A genuine pileup is more urgent than the same number of separated
    // zombies, but the stack must fit inside one lawn cell.
    return score + LargestZombieStackInRow(state, row) * 90;
}

int MostUrgentCounterRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = 0;
    for (int row = 0; row < state.rows; ++row) {
        const int score = CounterPressureScoreInRow(state, row);
        if (score > bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

int ZombieThreatWeight(std::uint16_t zombieType) {
    switch (static_cast<ZombieType>(zombieType)) {
        case ZombieType::ZOMBIE_GIGA_GARGANTUAR:
        case ZombieType::ZOMBIE_GARGANTUAR:
        case ZombieType::ZOMBIE_GIGA_FOOTBALL:
            return 115;
        case ZombieType::ZOMBIE_BOBSLED:
        case ZombieType::ZOMBIE_ZAMBONI:
        case ZombieType::ZOMBIE_FOOTBALL:
        case ZombieType::ZOMBIE_WALLNUT_HEAD:
            return 80;
        case ZombieType::ZOMBIE_PAIL:
        case ZombieType::ZOMBIE_DIGGER:
        case ZombieType::ZOMBIE_POLEVAULTER:
            return 55;
        default:
            return 30;
    }
}

int ZombieFrontlineValueInRow(const VSGameState &state, int row) {
    int score = 0;
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        score += ZombieThreatWeight(zombie.zombieType);
        score += IsHeavyZombie(zombie.zombieType) ? 70 : 0;
        score += zombie.shieldHealth > 0 ? 20 : 0;
        score += zombie.eating ? 35 : 0;
        score += zombie.positionX < 760.0f ? 25 : 0;
    }
    return score;
}

int MostValuableZombieFrontRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        const int score = ZombieFrontlineValueInRow(state, row);
        if (score > bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

int ZombiePressureInRow(const VSGameState &state, int row) {
    int pressure = 0;
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        // Spawned zombies are already an investment. Penalize a saturated lane
        // while still accounting for a heavy or advancing zombie that must be
        // supported immediately.
        pressure += 55 + std::clamp((900 - static_cast<int>(zombie.positionX)) / 10, 0, 65);
        pressure += IsHeavyZombie(zombie.zombieType) ? 25 : 0;
        pressure += zombie.eating ? 40 : 0;
    }
    return pressure;
}

int PlantDefenseValue(const VSPlantState &plant) {
    const int healthRatio = plant.maxHealth > 0 ? std::clamp(plant.health * 100 / plant.maxHealth, 0, 100) : 50;
    int score = 0;
    switch (static_cast<SeedType>(plant.seedType)) {
        case SeedType::SEED_WALLNUT:
        case SeedType::SEED_TALLNUT:
        case SeedType::SEED_PUMPKINSHELL:
            score = 110;
            break;
        case SeedType::SEED_SNOWPEA:
            score = 75;
            break;
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_SPLITPEA:
            score = 45;
            break;
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
            score = 65;
            break;
        case SeedType::SEED_THREEPEATER:
            score = 75;
            break;
        case SeedType::SEED_MELONPULT:
            score = 95;
            break;
        case SeedType::SEED_WINTERMELON:
            score = 115;
            break;
        case SeedType::SEED_GATLINGPEA:
        case SeedType::SEED_GLOOMSHROOM:
            score = 110;
            break;
        case SeedType::SEED_BONK_CHOY:
        case SeedType::SEED_CELERY_STALKER:
            score = 65;
            break;
        case SeedType::SEED_IMP_PEAR:
            score = 55;
            break;
        case SeedType::SEED_STARFRUIT:
        case SeedType::SEED_SPORESHROOM:
            score = 70;
            break;
        case SeedType::SEED_CHOMPER:
            score = 60;
            break;
        default:
            score = 25;
            break;
    }
    return score * healthRatio / 100;
}

struct PlantLaneAssessment {
    int row = 0;
    int danger = 0;
    int rawDanger = 0;
    int defense = 0;
    int plantCount = 0;
    bool hasHeavy = false;
    bool hasFast = false;
    const VSZombieState *closest = nullptr;
};

PlantLaneAssessment AssessPlantLane(const VSGameState &state, int row) {
    PlantLaneAssessment assessment{};
    assessment.row = row;
    assessment.closest = FindClosestZombie(state, row);
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        const int advance = std::clamp((850 - static_cast<int>(zombie.positionX)) / 4, 0, 190);
        assessment.rawDanger += ZombieThreatWeight(zombie.zombieType) + advance;
        assessment.rawDanger += zombie.eating ? 135 : 0;
        assessment.rawDanger += zombie.positionX < 560.0f ? 55 : 0;
        assessment.rawDanger += zombie.positionX < 400.0f ? 85 : 0;
        assessment.rawDanger += std::min(40, std::max(0, zombie.shieldHealth) / 30);
        assessment.hasHeavy = assessment.hasHeavy || IsHeavyZombie(zombie.zombieType);
        assessment.hasFast = assessment.hasFast || IsFastZombie(zombie.zombieType);
    }

    for (const VSPlantState &plant : state.plants) {
        if (!IsDeadOrOutside(plant) && plant.position.row == row) {
            assessment.defense += PlantDefenseValue(plant);
            ++assessment.plantCount;
        } else if (!IsDeadOrOutside(plant) && plant.seedType == static_cast<std::uint16_t>(SeedType::SEED_STARFRUIT)
                   && std::abs(static_cast<int>(plant.position.row) - row) == 1) {
            // Starfruit's diagonal shots support both adjacent lanes. Treat
            // that fire as partial cover instead of repeatedly overbuilding
            // a lane next to an established Starfruit pattern.
            assessment.defense += PlantDefenseValue(plant) / 2;
        }
    }
    assessment.danger = std::max(0, assessment.rawDanger - assessment.defense / 2);
    return assessment;
}

PlantLaneAssessment MostThreatenedPlantLane(const VSGameState &state) {
    PlantLaneAssessment best{};
    best.danger = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        const PlantLaneAssessment assessment = AssessPlantLane(state, row);
        if (assessment.danger > best.danger) {
            best = assessment;
        }
    }
    return best;
}

bool IsPlantEconomySeed(const VSGameState &state, std::uint16_t seedType);

int LeastDevelopedPlantRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::max();
    for (int row = 0; row < state.rows; ++row) {
        const PlantLaneAssessment assessment = AssessPlantLane(state, row);
        const int incomeCount = static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [&state, row](const VSPlantState &plant) {
            return !IsDeadOrOutside(plant) && plant.position.row == row && IsPlantEconomySeed(state, plant.seedType);
        }));
        const int score = assessment.defense + assessment.plantCount * 12 + incomeCount * 15;
        if (score < bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

int PlantValueScore(const VSPlantState &plant) {
    // Health is intentionally capped: a full Wall-nut should be a worthwhile target,
    // not erase every other lane from the zombie agent's comparison.
    int score = std::clamp(plant.health / 10, 10, 80);
    switch (static_cast<SeedType>(plant.seedType)) {
        case SeedType::SEED_SUNFLOWER:
        case SeedType::SEED_SUNSHROOM:
            score += 35;
            break;
        case SeedType::SEED_SNOWPEA:
            score += 90;
            break;
        case SeedType::SEED_GATLINGPEA:
        case SeedType::SEED_WINTERMELON:
        case SeedType::SEED_GLOOMSHROOM:
            score += 135;
            break;
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_REPEATER:
            score += 95;
            break;
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
            score += 75;
            break;
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_SPLITPEA:
            score += 55;
            break;
        case SeedType::SEED_BONK_CHOY:
        case SeedType::SEED_CELERY_STALKER:
            score += 75;
            break;
        case SeedType::SEED_STARFRUIT:
            score += 100;
            break;
        case SeedType::SEED_CHOMPER:
            score += 95;
            break;
        case SeedType::SEED_SPORESHROOM:
            score += 85;
            break;
        case SeedType::SEED_WALLNUT:
        case SeedType::SEED_TALLNUT:
        case SeedType::SEED_PUMPKINSHELL:
            score += 55;
            break;
        default:
            score += 45;
            break;
    }
    return score;
}

bool IsPlantEconomySeed(const VSGameState &state, std::uint16_t seedType) {
    return seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNFLOWER)
        || seedType == static_cast<std::uint16_t>(SeedType::SEED_TWINSUNFLOWER)
        || (state.isNight && seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNSHROOM));
}

bool IsPlantCombatSeed(std::uint16_t seedType) {
    switch (static_cast<SeedType>(seedType)) {
        case SeedType::SEED_SNOWPEA:
        case SeedType::SEED_BONK_CHOY:
        case SeedType::SEED_CELERY_STALKER:
        case SeedType::SEED_CHOMPER:
        case SeedType::SEED_STARFRUIT:
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_GLOOMSHROOM:
        case SeedType::SEED_SPORESHROOM:
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_WINTERMELON:
        case SeedType::SEED_GATLINGPEA:
        case SeedType::SEED_COBCANNON:
            return true;
        default:
            return false;
    }
}

bool IsSustainedOutputSeed(SeedType seedType) {
    switch (seedType) {
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_SNOWPEA:
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_STARFRUIT:
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_SPORESHROOM:
        case SeedType::SEED_GATLINGPEA:
        case SeedType::SEED_WINTERMELON:
        case SeedType::SEED_GLOOMSHROOM:
            return true;
        default:
            return false;
    }
}

int SustainedOutputValue(SeedType seedType) {
    switch (seedType) {
        case SeedType::SEED_GATLINGPEA:
        case SeedType::SEED_WINTERMELON:
        case SeedType::SEED_GLOOMSHROOM:
            return 130;
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_STARFRUIT:
            return 100;
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_SNOWPEA:
        case SeedType::SEED_SPORESHROOM:
            return 80;
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_SPLITPEA:
            return 55;
        default:
            return 0;
    }
}

int CountSustainedOutputPlants(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && IsSustainedOutputSeed(static_cast<SeedType>(plant.seedType));
    }));
}

int SustainedOutputScoreInRow(const VSGameState &state, int row) {
    int score = 0;
    for (const VSPlantState &plant : state.plants) {
        if (IsDeadOrOutside(plant) || plant.position.row != row) {
            continue;
        }

        const SeedType seed = static_cast<SeedType>(plant.seedType);
        int plantScore = SustainedOutputValue(seed);
        if (seed == SeedType::SEED_BONK_CHOY || seed == SeedType::SEED_CELERY_STALKER) {
            plantScore = 55;
        } else if (seed == SeedType::SEED_CHOMPER) {
            plantScore = 65;
        }
        if (plantScore == 0) {
            continue;
        }
        const int healthRatio = plant.maxHealth > 0 ? std::clamp(plant.health * 100 / plant.maxHealth, 0, 100) : 50;
        // A sleeping mushroom is an investment, but does not yet hold a lane.
        score += plantScore * (plant.asleep ? 25 : healthRatio) / 100;
    }
    return score;
}

int PlantEconomyValueInRow(const VSGameState &state, int row) {
    int score = 0;
    for (const VSPlantState &plant : state.plants) {
        if (IsDeadOrOutside(plant) || plant.position.row != row || !IsPlantEconomySeed(state, plant.seedType)) {
            continue;
        }
        const int healthRatio = plant.maxHealth > 0 ? std::clamp(plant.health * 100 / plant.maxHealth, 0, 100) : 50;
        score += 70 * healthRatio / 100;
        // A rear economy plant takes longer to replace than a disposable
        // front filler and is a better route to protect with lasting fire.
        score += std::max(0, 3 - static_cast<int>(plant.position.col)) * 8;
    }
    return score;
}

bool HasSustainedOutputSeed(const VSGameState &state) {
    return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [](const VSCardState &card) {
        return IsSustainedOutputSeed(static_cast<SeedType>(card.seedType));
    });
}

bool IsZombieEconomyItem(std::uint16_t gridItemType) {
    return gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_GRAVESTONE)
        || gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_BURIAL_MOUND);
}

int EstimatedEconomyMaxHealth(const VSGridItemState &item) {
    if (item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_BURIAL_MOUND)) {
        return 350 + 70 * (std::clamp(item.level, 0, 4) + 1);
    }
    return 350;
}

int StraightProjectileThreatToEconomy(const VSPlantState &plant, const VSGridItemState &economy) {
    const int rowDistance = std::abs(static_cast<int>(plant.position.row) - static_cast<int>(economy.position.row));
    if (plant.position.col >= economy.position.col) {
        return 0;
    }

    const SeedType seed = static_cast<SeedType>(plant.seedType);
    const bool reachesEconomyRow = rowDistance == 0 || (seed == SeedType::SEED_THREEPEATER && rowDistance == 1);
    if (!reachesEconomyRow) {
        return 0;
    }

    switch (seed) {
        case SeedType::SEED_GATLINGPEA:
            return 190;
        case SeedType::SEED_REPEATER:
            return 165;
        case SeedType::SEED_SNOWPEA:
            return 150;
        case SeedType::SEED_THREEPEATER:
            return 135;
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_CACTUS:
            return 120;
        default:
            return 0;
    }
}

int PlantThreatToEconomy(const VSPlantState &plant, const VSGridItemState &economy) {
    if (IsDeadOrOutside(plant) || plant.position.row < 0 || economy.position.row < 0) {
        return 0;
    }

    const int rowDistance = std::abs(static_cast<int>(plant.position.row) - static_cast<int>(economy.position.row));
    const int columnDistance = std::abs(static_cast<int>(plant.position.col) - static_cast<int>(economy.position.col));
    const SeedType seed = static_cast<SeedType>(plant.seedType);
    if (seed == SeedType::SEED_STARFRUIT) {
        // Starfruit fires in five directions and is the one plant that can
        // threaten a grave from an adjacent row as well as its own row.
        return rowDistance == 0 ? 145 : (rowDistance == 1 ? 75 : 0);
    }
    if (seed == SeedType::SEED_BONK_CHOY || seed == SeedType::SEED_CELERY_STALKER) {
        return rowDistance == 0 && columnDistance <= 2 ? 125 : 0;
    }
    if (seed == SeedType::SEED_GRAVEBUSTER) {
        return rowDistance == 0 && columnDistance == 0 ? 250 : 0;
    }
    if (const int projectileThreat = StraightProjectileThreatToEconomy(plant, economy); projectileThreat > 0) {
        return projectileThreat;
    }
    if (IsPlantCombatSeed(plant.seedType)) {
        return rowDistance == 0 && plant.position.col < economy.position.col ? 45 : 0;
    }
    return 0;
}

int StraightProjectileThreatScore(const VSGameState &state, int row) {
    int score = 0;
    for (const VSGridItemState &item : state.gridItems) {
        if (item.dead || !IsZombieEconomyItem(item.gridItemType) || item.position.row != row) {
            continue;
        }
        for (const VSPlantState &plant : state.plants) {
            if (!IsDeadOrOutside(plant)) {
                score += StraightProjectileThreatToEconomy(plant, item);
            }
        }
    }
    return score;
}

int GraveThreatScore(const VSGameState &state, int row) {
    int score = 0;
    for (const VSGridItemState &item : state.gridItems) {
        if (item.dead || !IsZombieEconomyItem(item.gridItemType) || item.position.row != row) {
            continue;
        }

        const int maxHealth = std::max(1, EstimatedEconomyMaxHealth(item));
        const int health = std::clamp(item.health, 0, maxHealth);
        score += std::max(0, (maxHealth - health) * 100 / maxHealth);
        score += health <= maxHealth / 3 ? 100 : (health <= maxHealth / 2 ? 45 : 0);
        for (const VSPlantState &plant : state.plants) {
            score += PlantThreatToEconomy(plant, item);
        }
    }
    return score;
}

int ZombieEconomyAssetValue(const VSGridItemState &item) {
    if (!IsZombieEconomyItem(item.gridItemType)) {
        return 0;
    }
    if (item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_BURIAL_MOUND)) {
        return 135 + std::clamp(item.level, 0, 4) * 90;
    }
    return 110;
}

int ZombieEconomyAttackOpportunity(const VSGameState &state, int row) {
    int score = 0;
    for (const VSGridItemState &item : state.gridItems) {
        if (item.dead || item.position.row != row || !IsZombieEconomyItem(item.gridItemType)) {
            continue;
        }

        const int assetValue = ZombieEconomyAssetValue(item);
        const int maxHealth = std::max(1, EstimatedEconomyMaxHealth(item));
        int existingPressure = 0;
        for (const VSPlantState &plant : state.plants) {
            existingPressure += PlantThreatToEconomy(plant, item);
        }
        // A fresh grave/mound is worth opening a firing lane for.  Once it is
        // already under fire, finishing it remains useful but needs fewer
        // additional resources than a completely untouched income source.
        score += existingPressure > 0 ? assetValue : assetValue * 3;
        if (item.health <= maxHealth / 2) {
            score += assetValue / 2;
        }
    }
    return score;
}

int SeedEconomyPressureOpportunity(const VSGameState &state, SeedType seed, int row) {
    int score = 0;
    for (const VSGridItemState &item : state.gridItems) {
        if (item.dead || !IsZombieEconomyItem(item.gridItemType)) {
            continue;
        }
        if (seed == SeedType::SEED_GRAVEBUSTER && item.gridItemType != static_cast<std::uint16_t>(GridItemType::GRIDITEM_GRAVESTONE)) {
            continue;
        }

        const int rowDistance = std::abs(row - static_cast<int>(item.position.row));
        int pressure = 0;
        if (seed == SeedType::SEED_GRAVEBUSTER) {
            pressure = rowDistance == 0 ? 4 : 0;
        } else if (seed == SeedType::SEED_STARFRUIT) {
            pressure = rowDistance == 0 ? 3 : (rowDistance == 1 ? 2 : 0);
        } else if (seed == SeedType::SEED_THREEPEATER) {
            pressure = rowDistance <= 1 ? 2 : 0;
        } else if (IsSustainedOutputSeed(seed)) {
            pressure = rowDistance == 0 ? 2 : 0;
        }
        score += pressure * ZombieEconomyAssetValue(item);
    }
    return score;
}

int MostVulnerableZombieEconomyRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        const int score = ZombieEconomyAttackOpportunity(state, row);
        if (score > bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

int MostThreatenedEconomyRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        const int score = GraveThreatScore(state, row);
        if (score > bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

int LeastThreatenedEconomyRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::max();
    for (int row = 0; row < state.rows; ++row) {
        const int score = GraveThreatScore(state, row);
        if (score < bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

int PlantLaneWeaknessScore(const VSGameState &state, int row) {
    const PlantLaneAssessment assessment = AssessPlantLane(state, row);
    if (assessment.plantCount == 0) {
        return -20;
    }

    int economyPlants = 0;
    int combatPlants = 0;
    int highValuePlants = 0;
    for (const VSPlantState &plant : state.plants) {
        if (IsDeadOrOutside(plant) || plant.position.row != row) {
            continue;
        }
        economyPlants += IsPlantEconomySeed(state, plant.seedType) ? 1 : 0;
        combatPlants += IsPlantCombatSeed(plant.seedType) ? 1 : 0;
        highValuePlants += PlantValueScore(plant) >= 100 ? 1 : 0;
    }

    // A line with multiple Sunflowers is a real investment.  It must outrank
    // a merely sparse line so zombies keep opening distinct economic fronts.
    int score = assessment.plantCount * 14 + economyPlants * 95 + std::max(0, economyPlants - 1) * 45 + highValuePlants * 24;
    score += std::max(0, 120 - assessment.defense);
    score += combatPlants == 0 ? 35 : 0;
    score += assessment.rawDanger / 4;
    return score;
}

int EconomyPlantsInRow(const VSGameState &state, int row) {
    return static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [&state, row](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.position.row == row && IsPlantEconomySeed(state, plant.seedType);
    }));
}

int ZombieLaneAttackScore(const VSGameState &state, int row) {
    const PlantLaneAssessment assessment = AssessPlantLane(state, row);
    const int zombieCount = CountZombiesInRow(state, row);
    const int economyPlants = EconomyPlantsInRow(state, row);
    const int graveThreat = GraveThreatScore(state, row);
    int score = PlantLaneWeaknessScore(state, row);

    // Sunflowers and other economy plants are the most efficient pressure
    // targets. Empty rows are still useful for forcing the plant player to
    // spend resources, but are less valuable than a developed economy lane.
    score += economyPlants * 150 + std::max(0, economyPlants - 1) * 60;
    score += assessment.plantCount == 0 ? 28 : 0;
    score += assessment.defense < 100 ? 35 : 0;
    score += graveThreat * 3;

    // Spread the opening across lanes. A single zombie is useful as a probe;
    // additional zombies in that lane receive a progressively larger penalty.
    if (zombieCount == 0) {
        score += 95;
    } else if (zombieCount == 1) {
        score -= 35;
    } else {
        score -= 35 + (zombieCount - 1) * 125;
    }
    score -= ZombiePressureInRow(state, row) / 3;
    return score;
}

int MostVulnerablePlantRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        const int score = PlantLaneWeaknessScore(state, row);
        if (score > bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

VSGridPosition FindPlantCellInColumns(const VSGameState &state, int preferredRow, int firstColumn, int lastColumn) {
    preferredRow = std::clamp(preferredRow, 0, std::max(0, state.rows - 1));
    firstColumn = std::clamp(firstColumn, 0, 5);
    lastColumn = std::clamp(lastColumn, firstColumn, 5);
    for (int rowOffset = 0; rowOffset < state.rows; ++rowOffset) {
        const int row = (preferredRow + rowOffset) % state.rows;
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const VSGridPosition position{static_cast<std::int8_t>(column), static_cast<std::int8_t>(row)};
            if (!HasPlantAt(state, position) && !HasGridItemAt(state, position)) {
                return position;
            }
        }
    }
    return {};
}

VSGridPosition FindPlantCellInExactRow(const VSGameState &state, int row, int firstColumn, int lastColumn) {
    if (row < 0 || row >= state.rows) {
        return {};
    }
    firstColumn = std::clamp(firstColumn, 0, 5);
    lastColumn = std::clamp(lastColumn, firstColumn, 5);
    for (int column = firstColumn; column <= lastColumn; ++column) {
        const VSGridPosition position{static_cast<std::int8_t>(column), static_cast<std::int8_t>(row)};
        if (!HasPlantAt(state, position) && !HasGridItemAt(state, position)) {
            return position;
        }
    }
    return {};
}

bool IsIncomeRowSafe(const VSGameState &state, int row) {
    for (const VSZombieState &zombie : state.zombies) {
        // Do not invest in more income on a route whose front has already
        // crossed the zombie-side lawn boundary.  A Sunflower placed there
        // becomes an immediate target instead of a long-term investment.
        if (!zombie.dead && zombie.row == row && (zombie.eating || zombie.positionX < 720.0f)) {
            return false;
        }
    }
    return true;
}

bool IsRangedOutputTradeUnfavorable(const VSGameState &state, int row) {
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        // A door or trashcan is designed to win a straight projectile trade.
        // Put new repeatable fire on a different grave route and use a real
        // answer for this lane when it becomes urgent.
        switch (static_cast<ZombieType>(zombie.zombieType)) {
            case ZombieType::ZOMBIE_TRASHCAN:
            case ZombieType::ZOMBIE_DOOR:
            case ZombieType::ZOMBIE_WALLNUT_HEAD:
                return true;
            default:
                break;
        }
        if (zombie.eating || zombie.positionX < 640.0f) {
            return true;
        }
    }
    return false;
}

VSGridPosition FindSafeIncomeCell(const VSGameState &state, int preferredRow) {
    preferredRow = std::clamp(preferredRow, 0, std::max(0, state.rows - 1));
    for (int rowOffset = 0; rowOffset < state.rows; ++rowOffset) {
        const int row = (preferredRow + rowOffset) % state.rows;
        if (!IsIncomeRowSafe(state, row)) {
            continue;
        }
        for (int column = 0; column <= 2; ++column) {
            const VSGridPosition position{static_cast<std::int8_t>(column), static_cast<std::int8_t>(row)};
            if (!HasPlantAt(state, position) && !HasGridItemAt(state, position)) {
                return position;
            }
        }
    }
    return {};
}

int ZombiePlacementColumn(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_DANCER:
            // The Dancer needs room to summon its line of backups.
            return 7;
        case SeedType::SEED_ZOMBIE_CATAPULT:
        case SeedType::SEED_ZOMBIE_BALLOON:
            // Ranged/flying pressure is more useful when it starts protected.
            return 8;
        default:
            // Plants occupy 0..5; column 6 is the zombie front line.
            return 6;
    }
}

VSGridPosition FindZombieCell(const VSGameState &state, SeedType seed, int row) {
    row = std::clamp(row, 0, std::max(0, state.rows - 1));
    return {static_cast<std::int8_t>(ZombiePlacementColumn(seed)), static_cast<std::int8_t>(row)};
}

VSGridPosition FindZombieEconomyCell(const VSGameState &state, int preferredRow) {
    preferredRow = std::clamp(preferredRow, 0, std::max(0, state.rows - 1));
    for (int column = 8; column >= 6; --column) {
        for (int rowOffset = 0; rowOffset < state.rows; ++rowOffset) {
            const int row = (preferredRow + rowOffset) % state.rows;
            const VSGridPosition position{static_cast<std::int8_t>(column), static_cast<std::int8_t>(row)};
            if (!HasPlantAt(state, position) && !HasGridItemAt(state, position)) {
                return position;
            }
        }
    }
    return {};
}

VSGridPosition FindZombieMoundCell(const VSGameState &state, int row) {
    const VSGridItemState *bestItem = nullptr;
    int bestScore = std::numeric_limits<int>::min();
    const bool hasUpgradeableMound = std::any_of(state.gridItems.begin(), state.gridItems.end(), [](const VSGridItemState &item) {
        return !item.dead && item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_BURIAL_MOUND) && item.level < 4;
    });
    for (const VSGridItemState &item : state.gridItems) {
        if (item.dead || item.position.row != row) {
            continue;
        }

        int score = std::numeric_limits<int>::min();
        if (item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_GRAVESTONE)) {
            // Establish one mound first, then retain its accumulated upgrade
            // value instead of spreading every mound card across new graves.
            if (!hasUpgradeableMound) {
                score = 260;
            }
        } else if (item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_BURIAL_MOUND) && item.level < 4) {
            score = 360 + item.level * 80;
        }
        if (score > bestScore) {
            bestItem = &item;
            bestScore = score;
        }
    }
    return bestItem == nullptr ? VSGridPosition{} : bestItem->position;
}

int MoundUpgradeCostAt(const VSGameState &state, VSGridPosition position) {
    for (const VSGridItemState &item : state.gridItems) {
        if (item.dead || item.position.col != position.col || item.position.row != position.row) {
            continue;
        }
        if (item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_GRAVESTONE)) {
            // Converting a basic grave creates level zero, which still uses
            // the base mound card cost.
            return 75;
        }
        if (item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_BURIAL_MOUND)) {
            switch (item.level) {
                case 0:
                    return 150;
                case 1:
                    return 225;
                case 2:
                    return 300;
                case 3:
                    return 450;
                default:
                    return std::numeric_limits<int>::max();
            }
        }
    }
    return std::numeric_limits<int>::max();
}

bool IsReadyCard(const VSCardState &card, int resource);

bool IsCardReadyForZombieTarget(const VSCardState &card, const VSGameState &state, VSGridPosition target) {
    if (card.seedType != static_cast<std::uint16_t>(SeedType::SEED_ZOMBIE_MOUND)) {
        return IsReadyCard(card, state.zombieBrains);
    }
    return !card.matchRestricted && card.active && !card.refreshing && card.refreshCounter <= 0 && MoundUpgradeCostAt(state, target) <= state.zombieBrains;
}

int CountZombieEconomy(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.gridItems.begin(), state.gridItems.end(), [](const VSGridItemState &item) {
        return !item.dead && (item.gridItemType == GridItemType::GRIDITEM_GRAVESTONE || item.gridItemType == GridItemType::GRIDITEM_MP_BURIAL_MOUND);
    }));
}

int HeavyZombieEconomyThreshold(const VSGameState &state) {
    // The zombie economy occupies its three rear columns.  Keep at most two
    // positions open before committing to an expensive game-ending zombie.
    const int economyTarget = std::max(0, state.rows * 3);
    return std::max(0, economyTarget - std::min(2, std::max(0, state.rows - 1)));
}

int StrategyBucket(int value) {
    return value <= 0 ? 0 : value <= 2 ? 1 : value <= 4 ? 2 : 3;
}

int CountLivePlants(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [](const VSPlantState &plant) { return !IsDeadOrOutside(plant); }));
}

int CountPlantIncome(const VSGameState &state) {
    return CountPlantType(state, SeedType::SEED_SUNFLOWER)
        + (state.isNight ? CountPlantType(state, SeedType::SEED_SUNSHROOM) : 0);
}

struct StrategyRule {
    VSSide side = VSSide::Plants;
    std::uint16_t seed = 0;
    int phase = 0;
    std::array<int, 4> buckets = {-1, -1, -1, -1};
    int bonus = 0;
};

constexpr std::array<unsigned char, 8> kStrategyDatabaseMagic = {'P', 'V', 'Z', 'V', 'S', 'D', 'B', '\0'};
constexpr std::uint16_t kStrategyDatabaseVersion = 1;
constexpr std::size_t kStrategyDatabaseHeaderSize = 12;
constexpr std::size_t kStrategyDatabaseRuleSize = 12;

std::uint16_t ReadStrategyU16(const std::vector<unsigned char> &data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) | (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

class StrategyDatabase {
    std::vector<StrategyRule> mRules;
    bool mLoaded = false;

    void Load() {
        if (mLoaded || Sexy::gSexyAppBase == nullptr) {
            return;
        }
        mLoaded = true;

        Sexy::Buffer buffer;
        if (!Sexy::gSexyAppBase->ReadBufferFromFile("addonFiles/data/vs_ai_strategy_db.bin", &buffer, false)) {
            return;
        }

        const auto &data = buffer.mData;
        if (data.size() < kStrategyDatabaseHeaderSize || !std::equal(kStrategyDatabaseMagic.begin(), kStrategyDatabaseMagic.end(), data.begin())
            || ReadStrategyU16(data, 8) != kStrategyDatabaseVersion) {
            return;
        }
        const std::size_t ruleCount = ReadStrategyU16(data, 10);
        if (ruleCount > (data.size() - kStrategyDatabaseHeaderSize) / kStrategyDatabaseRuleSize
            || kStrategyDatabaseHeaderSize + ruleCount * kStrategyDatabaseRuleSize != data.size()) {
            return;
        }

        for (std::size_t index = 0; index < ruleCount; ++index) {
            const std::size_t offset = kStrategyDatabaseHeaderSize + index * kStrategyDatabaseRuleSize;
            const unsigned char sideCode = data[offset];
            const int phase = data[offset + 3];
            const int bonus = data[offset + 8];
            if (sideCode > 1 || phase < 0 || phase > 2 || bonus <= 0 || bonus > 100) {
                continue;
            }

            StrategyRule rule{};
            rule.side = sideCode == 0 ? VSSide::Plants : VSSide::Zombies;
            rule.seed = ReadStrategyU16(data, offset + 1);
            rule.phase = phase;
            bool validRule = true;
            for (std::size_t bucketIndex = 0; bucketIndex < rule.buckets.size(); ++bucketIndex) {
                const int bucket = static_cast<int>(static_cast<std::int8_t>(data[offset + 4 + bucketIndex]));
                if (bucket < -1 || bucket > 3) {
                    validRule = false;
                    break;
                }
                rule.buckets[bucketIndex] = bucket;
            }
            if (!validRule) {
                continue;
            }
            rule.bonus = bonus;
            mRules.push_back(rule);
        }
    }

public:
    int Bonus(const VSGameState &state, VSSide side, SeedType seed, int targetRow) {
        Load();
        if (mRules.empty() || targetRow < 0 || targetRow >= state.rows) {
            return 0;
        }

        const int ownEconomy = side == VSSide::Plants ? CountPlantIncome(state) : CountZombieEconomy(state);
        const int opponentUnits = side == VSSide::Plants ? CountActiveZombies(state) : CountLivePlants(state);
        const int ownLaneUnits = side == VSSide::Plants ? CountPlantsInRow(state, targetRow) : CountZombiesInRow(state, targetRow);
        const int opponentLaneUnits = side == VSSide::Plants ? CountZombiesInRow(state, targetRow) : CountPlantsInRow(state, targetRow);
        const int totalLiveUnits = CountLivePlants(state) + CountActiveZombies(state);
        const int phase = ownEconomy < 3 && totalLiveUnits < 11 ? 0 : totalLiveUnits < 25 ? 1 : 2;
        const std::array<int, 4> buckets = {
            StrategyBucket(ownEconomy),
            StrategyBucket(opponentUnits),
            StrategyBucket(ownLaneUnits),
            StrategyBucket(opponentLaneUnits),
        };

        int bestBonus = 0;
        for (const StrategyRule &rule : mRules) {
            if (rule.side != side || rule.seed != static_cast<std::uint16_t>(seed) || rule.phase != phase) {
                continue;
            }
            bool matches = true;
            for (std::size_t index = 0; index < buckets.size(); ++index) {
                matches = matches && (rule.buckets[index] < 0 || rule.buckets[index] == buckets[index]);
            }
            if (matches) {
                bestBonus = std::max(bestBonus, rule.bonus);
            }
        }
        return bestBonus;
    }
};

int StrategyBonus(const VSGameState &state, VSSide side, SeedType seed, int targetRow) {
    static StrategyDatabase database;
    return database.Bonus(state, side, seed, targetRow);
}

bool IsReadyCard(const VSCardState &card, int resource) {
    return card.seedType != static_cast<std::uint16_t>(SeedType::SEED_NONE) && !card.matchRestricted && card.active && !card.refreshing
        && card.refreshCounter <= 0 && card.cost <= resource;
}

bool IsAreaCounterSeed(SeedType seed);
bool IsZombieGraveGuardSeed(SeedType seed);

int ReadyPlantAreaCounterCount(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.seedBanks[0].begin(), state.seedBanks[0].end(), [&state](const VSCardState &card) {
        return IsAreaCounterSeed(static_cast<SeedType>(card.seedType)) && IsReadyCard(card, state.plantSun);
    }));
}

int PlantAreaCounterExposure(const VSGameState &state, int row) {
    const int readyCounters = ReadyPlantAreaCounterCount(state);
    const VSZombieState *closest = FindClosestZombie(state, row);
    if (readyCounters == 0 || closest == nullptr) {
        return 0;
    }

    const int zombieCount = CountZombiesInRow(state, row);
    const int stackCount = LargestZombieStackInRow(state, row);
    int score = 0;
    if (zombieCount >= 2) {
        score += 130 + (zombieCount - 2) * 90;
    }
    if (stackCount >= 2) {
        score += 150 + (stackCount - 2) * 120;
    }
    // Once a front reaches the plant half, its exact position is already a
    // legal Squash/Cherry target.  Do not make that trade easier for plants.
    if (closest->positionX < 760.0f) {
        score += 110;
    }
    return score * std::min(readyCounters, 2);
}

bool HasReadyZombieGraveGuard(const VSGameState &state) {
    return std::any_of(state.seedBanks[1].begin(), state.seedBanks[1].end(), [&state](const VSCardState &card) {
        return IsZombieGraveGuardSeed(static_cast<SeedType>(card.seedType)) && IsReadyCard(card, state.zombieBrains);
    });
}

bool IsAreaCounterSeed(SeedType seed) {
    return seed == SeedType::SEED_SQUASH || seed == SeedType::SEED_CHERRYBOMB || seed == SeedType::SEED_JALAPENO
        || seed == SeedType::SEED_ICESHROOM || seed == SeedType::SEED_DOOMSHROOM;
}

bool IsZombieBreakthroughSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_BOBSLED:
        case SeedType::SEED_ZOMBONI:
        case SeedType::SEED_ZOMBIE_FOOTBALL:
        case SeedType::SEED_ZOMBIE_GARGANTUAR:
        case SeedType::SEED_ZOMBIE_GIGA_FOOTBALL:
        case SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR:
        case SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER:
            return true;
        default:
            return false;
    }
}

bool HasReadyZombieBreakthroughCard(const VSGameState &state) {
    return std::any_of(state.seedBanks[1].begin(), state.seedBanks[1].end(), [&state](const VSCardState &card) {
        return IsZombieBreakthroughSeed(static_cast<SeedType>(card.seedType)) && IsReadyCard(card, state.zombieBrains);
    });
}

bool IsHeavyZombieSeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_GARGANTUAR || seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR
        || seed == SeedType::SEED_ZOMBIE_GIGA_FOOTBALL || seed == SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER;
}

bool IsLateGameHeavyZombieSeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_GARGANTUAR || seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR
        || seed == SeedType::SEED_ZOMBIE_GIGA_FOOTBALL;
}

bool IsZombieGraveGuardSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_TRASHCAN:
        case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
        case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
        case SeedType::SEED_ZOMBIE_PAIL:
            return true;
        default:
            return false;
    }
}

bool HasZombieGraveGuardInRow(const VSGameState &state, int row) {
    return HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_TRASHCAN)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_DOOR)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_WALLNUT_HEAD)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_PAIL);
}

class BuiltinVSAgent : public IVSAgent {
protected:
    std::uint16_t mSequence = 0;
    std::array<std::uint8_t, 32> mBlockedSlots{};

    void AdvanceBlockedSlots() {
        for (std::uint8_t &blocked : mBlockedSlots) {
            if (blocked > 0) {
                --blocked;
            }
        }
    }

    bool IsSlotBlocked(std::uint8_t slot) const {
        return slot < mBlockedSlots.size() && mBlockedSlots[slot] != 0;
    }

    void BlockSlot(std::uint8_t slot) {
        if (slot < mBlockedSlots.size()) {
            mBlockedSlots[slot] = 4;
        }
    }

    VSAction MakePlayAction(VSSide side, const VSCardState &card, VSGridPosition target, std::uint32_t tick) {
        return {
            .side = side,
            .kind = VSActionKind::PlaySeed,
            .seedSlot = card.slot,
            .expectedSeedType = card.seedType,
            .target = target,
            .notBeforeTick = tick,
            .expiresAtTick = tick + 120,
            .sequence = ++mSequence,
        };
    }

public:
    void Reset() override {
        mSequence = 0;
        mBlockedSlots.fill(0);
    }

    void OnActionResult(const VSAction &action, VSActionResult result) override {
        if (result == VSActionResult::RejectedInvalidTarget || result == VSActionResult::RejectedUnsupported || result == VSActionResult::RejectedCardUnavailable) {
            BlockSlot(action.seedSlot);
        }
    }
};

class PlantVSAgent final : public BuiltinVSAgent {
    const VSCardState *FindReadyCard(const VSGameState &state, SeedType seedType) const {
        for (const VSCardState &card : state.seedBanks[0]) {
            if (!IsSlotBlocked(card.slot) && card.seedType == static_cast<std::uint16_t>(seedType) && IsReadyCard(card, state.plantSun)) {
                return &card;
            }
        }
        return nullptr;
    }

    std::optional<VSAction> TryPlant(const VSGameState &state, SeedType seedType, int row, int firstColumn, int lastColumn) {
        const VSCardState *card = FindReadyCard(state, seedType);
        if (card == nullptr) {
            return std::nullopt;
        }
        const VSGridPosition target = FindPlantCellInColumns(state, row, firstColumn, lastColumn);
        if (target.col < 0 || target.row < 0) {
            return std::nullopt;
        }
        return MakePlayAction(VSSide::Plants, *card, target, state.boardTick);
    }

    std::optional<VSAction> TryPlantExactRow(const VSGameState &state, SeedType seedType, int row, int firstColumn, int lastColumn) {
        const VSCardState *card = FindReadyCard(state, seedType);
        if (card == nullptr) {
            return std::nullopt;
        }
        const VSGridPosition target = FindPlantCellInExactRow(state, row, firstColumn, lastColumn);
        if (target.col < 0 || target.row < 0) {
            return std::nullopt;
        }
        return MakePlayAction(VSSide::Plants, *card, target, state.boardTick);
    }

    std::optional<VSAction> TryCounterPlant(const VSGameState &state, SeedType seedType, int row, int firstColumn) {
        for (int column = 5; column >= firstColumn; --column) {
            if (std::optional<VSAction> action = TryPlantExactRow(state, seedType, row, column, column)) {
                return action;
            }
        }
        return std::nullopt;
    }

    std::optional<VSAction> TryIcebergLettuce(const VSGameState &state, int row, int protectedSun) {
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_ICEBERG_LETTUCE);
        const VSZombieState *closest = FindClosestZombie(state, row);
        if (card == nullptr || closest == nullptr || state.plantSun - card->cost < protectedSun
            || HasPlantTypeInRow(state, SeedType::SEED_ICEBERG_LETTUCE, row)) {
            return std::nullopt;
        }

        const PlantLaneAssessment lane = AssessPlantLane(state, row);
        const bool needsControl = IsFastZombie(closest->zombieType) || IsHeavyZombie(closest->zombieType) || lane.danger >= 135;
        const float triggerDistance = IsFastZombie(closest->zombieType) ? 760.0f : 680.0f;
        if (!needsControl || closest->positionX > triggerDistance) {
            return std::nullopt;
        }
        return TryCounterPlant(state, SeedType::SEED_ICEBERG_LETTUCE, row, 3);
    }

    std::optional<VSAction> TryTorchwoodSupport(const VSGameState &state, int protectedSun) {
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_TORCHWOOD);
        if (card == nullptr || state.plantSun - card->cost < protectedSun) {
            return std::nullopt;
        }

        int bestRow = -1;
        int bestScore = 0;
        for (int row = 0; row < state.rows; ++row) {
            if (HasPlantTypeInRow(state, SeedType::SEED_TORCHWOOD, row)) {
                continue;
            }
            const VSGridPosition target = FindPlantCellInExactRow(state, row, 3, 3);
            if (target.col < 0 || target.row < 0) {
                continue;
            }

            int peaFamilyCount = 0;
            for (const VSPlantState &plant : state.plants) {
                if (IsDeadOrOutside(plant) || plant.position.row != row || plant.position.col >= target.col) {
                    continue;
                }
                switch (static_cast<SeedType>(plant.seedType)) {
                    case SeedType::SEED_PEASHOOTER:
                    case SeedType::SEED_REPEATER:
                    case SeedType::SEED_THREEPEATER:
                    case SeedType::SEED_SPLITPEA:
                    case SeedType::SEED_GATLINGPEA:
                        ++peaFamilyCount;
                        break;
                    default:
                        break;
                }
            }
            const int score = peaFamilyCount * 180 + PlantEconomyValueInRow(state, row) / 2;
            if (peaFamilyCount > 0 && score > bestScore) {
                bestRow = row;
                bestScore = score;
            }
        }
        return bestRow < 0 ? std::nullopt : TryPlantExactRow(state, SeedType::SEED_TORCHWOOD, bestRow, 3, 3);
    }

    std::optional<VSAction> TryIncomePlant(const VSGameState &state, int row, int protectedSun) {
        if (state.isSuddenDeath) {
            return std::nullopt;
        }
        const VSGridPosition target = FindSafeIncomeCell(state, row);
        if (target.col < 0 || target.row < 0) {
            return std::nullopt;
        }
        const VSCardState *bestCard = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const SeedType seedType : {SeedType::SEED_SUNFLOWER, SeedType::SEED_SUNSHROOM}) {
            if (seedType == SeedType::SEED_SUNSHROOM && !state.isNight) {
                continue;
            }
            if (const VSCardState *card = FindReadyCard(state, seedType); card != nullptr) {
                if (state.plantSun - card->cost < protectedSun) {
                    continue;
                }
                const int score = StrategyBonus(state, VSSide::Plants, seedType, target.row);
                if (bestCard == nullptr || score > bestScore) {
                    bestCard = card;
                    bestScore = score;
                }
            }
        }
        return bestCard == nullptr ? std::nullopt : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *bestCard, target, state.boardTick));
    }

    std::optional<VSAction> TrySunshroomFiller(const VSGameState &state, int preferredRow, int protectedSun) {
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_SUNSHROOM);
        if (state.isSuddenDeath || state.isNight || card == nullptr || state.plantSun - card->cost < protectedSun) {
            return std::nullopt;
        }

        int bestRow = -1;
        int bestScore = std::numeric_limits<int>::min();
        for (int rowOffset = 0; rowOffset < state.rows; ++rowOffset) {
            const int row = (preferredRow + rowOffset) % state.rows;
            if (HasPlantTypeInRow(state, SeedType::SEED_SUNSHROOM, row)) {
                continue;
            }

            VSGridPosition target{};
            for (int column = 5; column >= 4; --column) {
                target = FindPlantCellInExactRow(state, row, column, column);
                if (target.col >= 0 && target.row >= 0) {
                    break;
                }
            }
            if (target.col < 0 || target.row < 0) {
                continue;
            }

            const PlantLaneAssessment lane = AssessPlantLane(state, row);
            const VSZombieState *closest = FindClosestZombie(state, row);
            if (closest == nullptr) {
                continue;
            }
            int score = lane.rawDanger * 2 + PlantEconomyValueInRow(state, row) * 2 + SustainedOutputScoreInRow(state, row);
            score += closest->positionX < 760.0f ? 140 : 0;
            score += row == preferredRow ? 35 : 0;
            if (bestRow < 0 || score > bestScore) {
                bestRow = row;
                bestScore = score;
            }
        }
        if (bestRow < 0) {
            return std::nullopt;
        }

        for (int column = 5; column >= 4; --column) {
            if (std::optional<VSAction> action = TryPlantExactRow(state, SeedType::SEED_SUNSHROOM, bestRow, column, column)) {
                return action;
            }
        }
        return std::nullopt;
    }

    std::optional<VSAction> TryGraveBuster(const VSGameState &state, int protectedSun) {
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_GRAVEBUSTER);
        if (card == nullptr || state.plantSun - card->cost < protectedSun) {
            return std::nullopt;
        }

        const VSGridItemState *bestItem = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const VSGridItemState &item : state.gridItems) {
            if (item.dead || item.gridItemType != static_cast<std::uint16_t>(GridItemType::GRIDITEM_GRAVESTONE)) {
                continue;
            }
            const PlantLaneAssessment lane = AssessPlantLane(state, item.position.row);
            const int maxHealth = std::max(1, EstimatedEconomyMaxHealth(item));
            int score = ZombieEconomyAssetValue(item) * 4;
            score += item.health <= maxHealth / 2 ? 80 : 0;
            score -= lane.danger * 2;
            if (bestItem == nullptr || score > bestScore) {
                bestItem = &item;
                bestScore = score;
            }
        }
        return bestItem == nullptr ? std::nullopt : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *card, bestItem->position, state.boardTick));
    }

    std::optional<VSAction> TrySustainedOutputPlant(const VSGameState &state, int row, int protectedSun) {
        const VSCardState *bestCard = nullptr;
        VSGridPosition bestTarget{};
        int bestScore = std::numeric_limits<int>::min();
        for (const VSCardState &card : state.seedBanks[0]) {
            if (IsSlotBlocked(card.slot) || !IsReadyCard(card, state.plantSun) || state.plantSun - card.cost < protectedSun) {
                continue;
            }

            const SeedType seed = static_cast<SeedType>(card.seedType);
            if (!IsSustainedOutputSeed(seed)) {
                continue;
            }

            for (int rowOffset = 0; rowOffset < state.rows; ++rowOffset) {
                const int targetRow = (row + rowOffset) % state.rows;
                if (seed == SeedType::SEED_SNOWPEA && HasPlantTypeInRow(state, seed, targetRow)) {
                    continue;
                }
                if (IsRangedOutputTradeUnfavorable(state, targetRow)) {
                    continue;
                }

                const VSGridPosition target = FindPlantCellInExactRow(state, targetRow, 1, 3);
                if (target.col < 0 || target.row < 0) {
                    continue;
                }

                const PlantLaneAssessment lane = AssessPlantLane(state, targetRow);
                const int existingOutput = SustainedOutputScoreInRow(state, targetRow);
                int score = SustainedOutputValue(seed) * 3 - card.cost / 3;
                // The recordings invest firepower behind established sun
                // rows.  This protects sunk economy while avoiding a full
                // one-lane turtle: existing output lowers the next score.
                score += PlantEconomyValueInRow(state, targetRow) * 2;
                score += std::max(0, 110 - existingOutput) * 2;
                score -= existingOutput / 2;
                // Once a firing lane reaches a grave, further shots convert
                // directly into lost zombie income.  That is worth more than
                // another safe Sunflower after the opening has stabilized.
                score += SeedEconomyPressureOpportunity(state, seed, targetRow) * 4;
                score += StrategyBonus(state, VSSide::Plants, seed, targetRow);
                // Local danger is handled by the counter branch. Durable
                // output belongs on a safe route where it can threaten the
                // grave economy instead of becoming a free blocker target.
                score -= lane.danger >= 105 ? 80 : 0;
                score += targetRow == row ? 25 : 0;
                if (seed == SeedType::SEED_SNOWPEA) {
                    score += 45;
                }
                if (seed == SeedType::SEED_STARFRUIT) {
                    const int centerDistance = std::abs(targetRow * 2 - (state.rows - 1));
                    score += 50 + (state.rows - centerDistance) * 8;
                }
                if (seed == SeedType::SEED_SPORESHROOM) {
                    score += 35;
                }
                if (bestCard == nullptr || score > bestScore) {
                    bestCard = &card;
                    bestTarget = target;
                    bestScore = score;
                }
            }
        }
        return bestCard == nullptr ? std::nullopt : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *bestCard, bestTarget, state.boardTick));
    }

    bool HasIncomeSeed(const VSGameState &state) const {
        if (state.isSuddenDeath) {
            return false;
        }
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [&state](const VSCardState &card) {
            return !card.matchRestricted && (card.seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNFLOWER)
                || (state.isNight && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNSHROOM)));
        });
    }

    bool HasSunshroomSeed(const VSGameState &state) const {
        return !state.isSuddenDeath && std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [](const VSCardState &card) {
            return !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNSHROOM);
        });
    }

    std::optional<VSAction> TryWakeSleepingMushroom(const VSGameState &state, int preferredRow) {
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_INSTANT_COFFEE);
        if (card == nullptr) {
            return std::nullopt;
        }

        const VSPlantState *bestPlant = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const VSPlantState &plant : state.plants) {
            if (IsDeadOrOutside(plant) || !plant.asleep || plant.seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNSHROOM)) {
                continue;
            }
            int score = PlantValueScore(plant) + (IsPlantCombatSeed(plant.seedType) ? 220 : 0);
            score += plant.position.row == preferredRow ? 70 : 0;
            if (bestPlant == nullptr || score > bestScore) {
                bestPlant = &plant;
                bestScore = score;
            }
        }
        return bestPlant == nullptr ? std::nullopt : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *card, bestPlant->position, state.boardTick));
    }

    std::optional<VSAction> TryPumpkinShell(const VSGameState &state, int row, int protectedSun) {
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_PUMPKINSHELL);
        if (card == nullptr || state.plantSun - card->cost < protectedSun) {
            return std::nullopt;
        }

        const VSPlantState *bestPlant = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const VSPlantState &plant : state.plants) {
            if (IsDeadOrOutside(plant) || plant.position.row != row || plant.seedType == static_cast<std::uint16_t>(SeedType::SEED_PUMPKINSHELL)
                || (!IsPlantCombatSeed(plant.seedType) && !IsPlantEconomySeed(state, plant.seedType))
                || HasPlantTypeAt(state, SeedType::SEED_PUMPKINSHELL, plant.position)) {
                continue;
            }
            const int score = PlantValueScore(plant) + (IsPlantCombatSeed(plant.seedType) ? 170 : 0) + static_cast<int>(plant.position.col) * 6;
            if (bestPlant == nullptr || score > bestScore) {
                bestPlant = &plant;
                bestScore = score;
            }
        }
        return bestPlant == nullptr ? std::nullopt : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *card, bestPlant->position, state.boardTick));
    }

    bool ShouldDeployWallnut(const VSGameState &state, int row) const {
        if (row < 0 || row >= state.rows || HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, row)
            || HasPlantTypeInRow(state, SeedType::SEED_TALLNUT, row)) {
            return false;
        }

        const VSZombieState *closest = FindClosestZombie(state, row);
        if (closest == nullptr) {
            return false;
        }

        bool hasProtectedInvestment = false;
        for (const VSPlantState &plant : state.plants) {
            if (IsDeadOrOutside(plant) || plant.position.row != row || plant.position.col > 3) {
                continue;
            }
            if (IsPlantEconomySeed(state, plant.seedType) || IsPlantCombatSeed(plant.seedType)) {
                hasProtectedInvestment = true;
                break;
            }
        }
        if (!hasProtectedInvestment) {
            return false;
        }

        // The nut belongs at the front only after an actual intruder reaches
        // the middle lawn.  A distant pail or trashcan is a reason to build
        // firepower, not to lock 50 sun into a premature wall.
        const bool decisive = IsDecisiveCounterZombie(closest->zombieType);
        const float triggerDistance = IsHeavyZombie(closest->zombieType) || decisive ? 620.0f : 550.0f;
        const PlantLaneAssessment lane = AssessPlantLane(state, row);
        return closest->eating || (closest->positionX <= triggerDistance && lane.danger >= (decisive ? 80 : 105));
    }

    int AreaCounterReserve(const VSGameState &state) const {
        int reserve = std::numeric_limits<int>::max();
        for (const VSCardState &card : state.seedBanks[0]) {
            if (IsSlotBlocked(card.slot) || !card.active || card.seedType == static_cast<std::uint16_t>(SeedType::SEED_NONE)) {
                continue;
            }
            if (IsAreaCounterSeed(static_cast<SeedType>(card.seedType))) {
                reserve = std::min(reserve, std::max(0, card.cost));
            }
        }
        return reserve == std::numeric_limits<int>::max() ? 0 : reserve;
    }

    std::optional<VSAction> TryFallbackPlant(const VSGameState &state, const PlantLaneAssessment &danger, int buildRow) {
        const bool hasActiveZombie = CountActiveZombies(state) > 0;
        for (const VSCardState &card : state.seedBanks[0]) {
            if (IsSlotBlocked(card.slot) || !IsReadyCard(card, state.plantSun)) {
                continue;
            }

            const SeedType seed = static_cast<SeedType>(card.seedType);
            if (seed == SeedType::SEED_SUNFLOWER || seed == SeedType::SEED_SUNSHROOM || seed == SeedType::SEED_IMP_PEAR
                || seed == SeedType::SEED_INSTANT_COFFEE || seed == SeedType::SEED_PUMPKINSHELL || seed == SeedType::SEED_ICEBERG_LETTUCE
                || seed == SeedType::SEED_TORCHWOOD) {
                continue;
            }
            const bool emergencySeed = seed == SeedType::SEED_SQUASH || seed == SeedType::SEED_CHERRYBOMB || seed == SeedType::SEED_JALAPENO
                || seed == SeedType::SEED_ICESHROOM || seed == SeedType::SEED_DOOMSHROOM;
            if (emergencySeed && (!hasActiveZombie || danger.danger < 150)) {
                continue;
            }

            // Instant counters are selected by their dedicated target logic;
            // they must never become generic emergency fillers in another lane.
            if (seed == SeedType::SEED_SQUASH || seed == SeedType::SEED_CHERRYBOMB) {
                continue;
            }

            if (seed == SeedType::SEED_SNOWPEA && HasPlantTypeInRow(state, seed, danger.danger >= 105 ? danger.row : buildRow)) {
                continue;
            }

            if ((seed == SeedType::SEED_WALLNUT || seed == SeedType::SEED_TALLNUT) && !ShouldDeployWallnut(state, danger.row)) {
                continue;
            }
            if (seed == SeedType::SEED_CHOMPER && (!hasActiveZombie || danger.closest == nullptr || danger.danger < 90)) {
                continue;
            }

            int row = danger.danger >= 105 ? danger.row : buildRow;
            if (IsSustainedOutputSeed(seed) && IsRangedOutputTradeUnfavorable(state, row)) {
                continue;
            }
            int firstColumn = 2;
            int lastColumn = 3;
            if (emergencySeed) {
                firstColumn = 4;
                lastColumn = 5;
            } else if (seed == SeedType::SEED_WALLNUT || seed == SeedType::SEED_TALLNUT) {
                firstColumn = lastColumn = 4;
            } else if (seed == SeedType::SEED_CHOMPER) {
                firstColumn = lastColumn = 4;
            } else if (seed == SeedType::SEED_BONK_CHOY || seed == SeedType::SEED_CELERY_STALKER) {
                firstColumn = lastColumn = 3;
            }

            const bool requiresExactRow = emergencySeed || seed == SeedType::SEED_CHOMPER;
            const VSGridPosition target = requiresExactRow ? FindPlantCellInExactRow(state, row, firstColumn, lastColumn)
                                                           : FindPlantCellInColumns(state, row, firstColumn, lastColumn);
            if (target.col >= 0 && target.row >= 0) {
                return MakePlayAction(VSSide::Plants, card, target, state.boardTick);
            }
        }
        return std::nullopt;
    }

public:
    std::optional<VSAction> Decide(const VSGameState &state) override {
        AdvanceBlockedSlots();
        for (const VSResourceState &resource : state.resources) {
            if (resource.side == VSSide::Plants && !resource.dead && !resource.beingCollected) {
                return VSAction{.side = VSSide::Plants, .kind = VSActionKind::CollectResource, .objectId = resource.id, .sequence = ++mSequence};
            }
        }

        const PlantLaneAssessment danger = MostThreatenedPlantLane(state);
        const int openingIncomeTarget = state.rows >= 6 ? 7 : 6;
        const int minimumIncomeBeforeOutput = state.rows >= 6 ? 4 : 3;
        const int incomePlantCount = CountPlantIncome(state);
        const int sustainedOutputCount = CountSustainedOutputPlants(state);
        const bool hasIncomeSeed = HasIncomeSeed(state);
        const bool hasSunshroomFiller = HasSunshroomSeed(state);
        const bool hasSustainedOutputSeed = HasSustainedOutputSeed(state);
        const bool hasActiveZombie = CountActiveZombies(state) > 0;
        const int counterRow = MostUrgentCounterRow(state);
        const VSZombieState *counterClosest = FindClosestZombie(state, counterRow);
        const int counterZombieCount = CountZombiesInRow(state, counterRow);
        const int counterStackCount = LargestZombieStackInRow(state, counterRow);
        const int counterCherryClusterCount = LargestCherryBombClusterInRow(state, counterRow);
        const PlantLaneAssessment counterLane = AssessPlantLane(state, counterRow);
        const int counterCombatPlants = static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [counterRow](const VSPlantState &plant) {
            return !IsDeadOrOutside(plant) && plant.position.row == counterRow && IsPlantCombatSeed(plant.seedType);
        }));
        const bool hasGargantuar = HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_GARGANTUAR)
            || HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_GIGA_GARGANTUAR);
        const bool hasGigaPoleVaulter = HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_GIGA_POLEVAULTER);
        const bool hasSquashPriorityZombie = HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_BOBSLED)
            || HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_ZAMBONI)
            || HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_FOOTBALL)
            || HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_GIGA_FOOTBALL);
        const bool earlySingleBucket = state.boardTick < 32000 && counterZombieCount == 1
            && HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_PAIL) && counterCombatPlants == 0 && counterLane.plantCount <= 2;
        const bool zombieCluster = hasActiveZombie && counterStackCount >= 2;
        const bool cherryThreat = counterClosest != nullptr && counterCherryClusterCount >= 2
            && (counterClosest->eating || counterClosest->positionX < 650.0f);
        const bool squashThreat = zombieCluster || hasSquashPriorityZombie || (hasGargantuar && counterClosest != nullptr
            && (counterClosest->eating || counterClosest->positionX < 560.0f)) || earlySingleBucket;
        const bool impPearThreat = (hasGargantuar || hasGigaPoleVaulter) && counterClosest != nullptr
            && (counterClosest->eating || counterClosest->positionX < 780.0f || counterLane.danger >= 160);
        const int areaCounterReserve = AreaCounterReserve(state);
        // During an active match, eleven income plants on a five-row lawn is
        // enough to fund the main damage line.  More Sunflowers are only a
        // good investment while the zombie side has left the board alone.
        const int incomeExpansionTarget = state.isSuddenDeath ? 0 : state.rows * 2 + (hasActiveZombie ? 1 : state.rows);
        const bool immediateCounterThreat = squashThreat || impPearThreat;
        const bool mustHoldCounterReserve = areaCounterReserve > 0 && state.plantSun >= areaCounterReserve
            && HasReadyZombieBreakthroughCard(state);
        const int protectedSun = mustHoldCounterReserve ? areaCounterReserve : 0;
        const int zombieEconomyStrikeRow = MostVulnerableZombieEconomyRow(state);
        const bool canStrikeZombieEconomy = (state.isSuddenDeath || incomePlantCount >= minimumIncomeBeforeOutput) && hasSustainedOutputSeed
            && CountZombieEconomy(state) > 0 && danger.danger < 150 && !immediateCounterThreat;
        // Every two economy plants should fund one durable attacker, up to a
        // line per row.  This prevents the old all-Sunflower opening from
        // leaving the board without enough repeatable damage.
        const int desiredOutputCount = state.isSuddenDeath ? state.rows : std::min(state.rows, std::max(1, (incomePlantCount + 1) / 2));
        const bool needsSustainedOutput = hasSustainedOutputSeed && sustainedOutputCount < desiredOutputCount;

        // The recorded plant side builds its sun base first, then answers a real
        // heavy/fast push with Squash. It is never an opening filler card.
        // Against Gargantuars the replay preserves Imp Pear for the first
        // answer; Squash is the follow-up when the giant reaches the line.
        if (hasActiveZombie && impPearThreat && !HasPlantTypeInRow(state, SeedType::SEED_IMP_PEAR, counterRow)) {
            if (std::optional<VSAction> action = TryCounterPlant(state, SeedType::SEED_IMP_PEAR, counterRow, 4)) {
                return action;
            }
        }
        if (cherryThreat && !HasPlantTypeInRow(state, SeedType::SEED_CHERRYBOMB, counterRow)) {
            if (std::optional<VSAction> action = TryCounterPlant(state, SeedType::SEED_CHERRYBOMB, counterRow, 3)) {
                return action;
            }
        }
        if (squashThreat && counterClosest != nullptr && !HasPlantTypeInRow(state, SeedType::SEED_SQUASH, counterRow)) {
            if (std::optional<VSAction> action = TryCounterPlant(state, SeedType::SEED_SQUASH, counterRow, 4)) {
                return action;
            }
        }
        if (hasActiveZombie) {
            if (std::optional<VSAction> action = TryIcebergLettuce(state, counterRow, protectedSun)) {
                return action;
            }
        }
        if (std::optional<VSAction> action = TryWakeSleepingMushroom(state, danger.row)) {
            return action;
        }

        // A healthy Sunflower count is not a win condition by itself.  When
        // the opposing grave economy is exposed, convert the available tempo
        // into direct pressure before investing another turn in own income.
        if (canStrikeZombieEconomy) {
            if (std::optional<VSAction> action = TryGraveBuster(state, protectedSun)) {
                return action;
            }
            if (hasSustainedOutputSeed) {
                if (std::optional<VSAction> action = TrySustainedOutputPlant(state, zombieEconomyStrikeRow, protectedSun)) {
                    return action;
                }
            }
        }

        if (!state.isNight && hasActiveZombie && hasSunshroomFiller && !immediateCounterThreat && counterClosest != nullptr
            && (incomePlantCount >= minimumIncomeBeforeOutput || sustainedOutputCount > 0)) {
            if (std::optional<VSAction> action = TrySunshroomFiller(state, danger.row, protectedSun)) {
                return action;
            }
        }

        if (zombieCluster && areaCounterReserve > 0 && state.plantSun < areaCounterReserve) {
            // Keep the remaining sun for the first available area answer. A
            // cheap shooter or nut cannot solve a multi-zombie pileup as well
            // as the reserved counter card.
            return std::nullopt;
        }

        if (!immediateCounterThreat && danger.danger < 105 && incomePlantCount >= 6 && sustainedOutputCount >= 3) {
            if (std::optional<VSAction> action = TryTorchwoodSupport(state, protectedSun)) {
                return action;
            }
        }

        if (hasIncomeSeed && incomePlantCount < openingIncomeTarget && danger.danger < 150) {
            if (incomePlantCount >= minimumIncomeBeforeOutput && needsSustainedOutput) {
                if (std::optional<VSAction> action = TrySustainedOutputPlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
                    return action;
                }
            }
            if (std::optional<VSAction> action = TryIncomePlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
                return action;
            }
            // Safe but unaffordable: wait for sun instead of spending a
            // defensive card merely because it is available.
            if (!hasActiveZombie || danger.danger < 90) {
                return std::nullopt;
            }
        }

        // The replay keeps adding Sunflowers after early probes arrive. Once
        // the opening base exists, continue that expansion whenever no lane
        // needs an instant counter instead of freezing income at six plants.
        const bool canExpandIncome = danger.danger < 105 || (counterCombatPlants > 0 && danger.danger < 140);
        if (hasIncomeSeed && hasActiveZombie && incomePlantCount < incomeExpansionTarget && !immediateCounterThreat && canExpandIncome) {
            if (needsSustainedOutput) {
                if (std::optional<VSAction> action = TrySustainedOutputPlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
                    return action;
                }
            }
            if (std::optional<VSAction> action = TryIncomePlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
                return action;
            }
        }

        if (!hasActiveZombie) {
            const int buildRow = LeastDevelopedPlantRow(state);
            if (hasIncomeSeed && incomePlantCount < minimumIncomeBeforeOutput) {
                if (std::optional<VSAction> action = TryIncomePlant(state, buildRow, protectedSun)) {
                    return action;
                }
            }
            if (needsSustainedOutput) {
                if (std::optional<VSAction> action = TrySustainedOutputPlant(state, buildRow, protectedSun)) {
                    return action;
                }
            }
            if (hasIncomeSeed && incomePlantCount < openingIncomeTarget + 2) {
                if (std::optional<VSAction> action = TryIncomePlant(state, buildRow, protectedSun)) {
                    return action;
                }
            }
            if (incomePlantCount >= 6 && sustainedOutputCount >= 3) {
                if (std::optional<VSAction> action = TryTorchwoodSupport(state, protectedSun)) {
                    return action;
                }
            }
            // Once the replay-like economy is established, pre-build only a
            // combat plant. Nuts and instant counters wait for a visible lane.
            if (std::optional<VSAction> action = TrySustainedOutputPlant(state, buildRow, protectedSun)) {
                return action;
            }
            if (!HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, buildRow)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_BONK_CHOY, buildRow, 3, 3)) {
                    return action;
                }
            }
            return TryFallbackPlant(state, danger, buildRow);
        }

        const bool hasRangedHarasser = HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_PEA_HEAD)
            || HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_SUNDAY_EDITION);
        if (danger.danger >= 85 && (hasRangedHarasser || SustainedOutputScoreInRow(state, danger.row) >= 55)) {
            if (std::optional<VSAction> action = TryPumpkinShell(state, danger.row, protectedSun)) {
                return action;
            }
        }

        if (danger.danger >= 105) {
            if (!IsRangedOutputTradeUnfavorable(state, danger.row) && !HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_SNOWPEA, danger.row, 1, 2)) {
                    return action;
                }
            }
            if (!HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_BONK_CHOY, danger.row, 3, 3)) {
                    return action;
                }
            }
            if (std::optional<VSAction> action = TryPumpkinShell(state, danger.row, protectedSun)) {
                return action;
            }
            if (ShouldDeployWallnut(state, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_WALLNUT, danger.row, 4, 4)) {
                    return action;
                }
            }
        }

        const int buildRow = LeastDevelopedPlantRow(state);
        if (!immediateCounterThreat && incomePlantCount >= 6 && sustainedOutputCount >= 3) {
            if (std::optional<VSAction> action = TryTorchwoodSupport(state, protectedSun)) {
                return action;
            }
        }
        if (std::optional<VSAction> action = TrySustainedOutputPlant(state, buildRow, protectedSun)) {
            return action;
        }
        if (!HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, buildRow)) {
            if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_BONK_CHOY, buildRow, 3, 3)) {
                return action;
            }
        }
        if (ShouldDeployWallnut(state, danger.row)) {
            if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_WALLNUT, danger.row, 4, 4)) {
                return action;
            }
        }
        if (hasIncomeSeed && incomePlantCount < incomeExpansionTarget) {
            return TryIncomePlant(state, buildRow, protectedSun);
        }
        return TryFallbackPlant(state, danger, buildRow);
    }
};

class ZombieVSAgent final : public BuiltinVSAgent {
    int mLastAttackRow = -1;

    static bool IsTargetedSeed(std::uint16_t seed) {
        const SeedType seedType = static_cast<SeedType>(seed);
        return seedType == SeedType::SEED_ZOMBIE_BUNGEE;
    }

    const VSCardState *FindReadyCard(const VSGameState &state, SeedType seedType) const {
        for (const VSCardState &card : state.seedBanks[1]) {
            if (IsSlotBlocked(card.slot) || card.seedType != static_cast<std::uint16_t>(seedType)) {
                continue;
            }
            if (seedType != SeedType::SEED_ZOMBIE_MOUND && IsReadyCard(card, state.zombieBrains)) {
                return &card;
            }
            if (seedType == SeedType::SEED_ZOMBIE_MOUND) {
                for (int row = 0; row < state.rows; ++row) {
                    const VSGridPosition target = FindZombieMoundCell(state, row);
                    if (target.col >= 0 && target.row >= 0 && IsCardReadyForZombieTarget(card, state, target)) {
                        return &card;
                    }
                }
            }
        }
        return nullptr;
    }

    int HeavyZombieReserve(const VSGameState &state) const {
        int reserve = std::numeric_limits<int>::max();
        for (const VSCardState &card : state.seedBanks[1]) {
            if (IsSlotBlocked(card.slot) || card.matchRestricted || !card.active || card.seedType == static_cast<std::uint16_t>(SeedType::SEED_NONE)) {
                continue;
            }
            if (IsLateGameHeavyZombieSeed(static_cast<SeedType>(card.seedType))) {
                reserve = std::min(reserve, std::max(0, card.cost));
            }
        }
        return reserve == std::numeric_limits<int>::max() ? 0 : reserve;
    }

    std::optional<VSAction> TryBuildEconomy(const VSGameState &state, int row) {
        if (state.isSuddenDeath) {
            return std::nullopt;
        }
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_ZOMBIE_GRAVESTONE);
        if (card == nullptr) {
            return std::nullopt;
        }
        const VSGridPosition target = FindZombieEconomyCell(state, row);
        if (target.col < 0 || target.row < 0) {
            return std::nullopt;
        }
        return MakePlayAction(VSSide::Zombies, *card, target, state.boardTick);
    }

    int LeastCommittedZombieRow(const VSGameState &state) const {
        int bestRow = 0;
        int bestScore = std::numeric_limits<int>::max();
        for (int row = 0; row < state.rows; ++row) {
            int economy = 0;
            for (const VSGridItemState &item : state.gridItems) {
                economy += !item.dead && item.position.row == row && IsZombieEconomyItem(item.gridItemType) ? 1 : 0;
            }
            const int score = economy * 130 + CountZombiesInRow(state, row) * 85 + GraveThreatScore(state, row) * 2;
            if (score < bestScore) {
                bestScore = score;
                bestRow = row;
            }
        }
        return bestRow;
    }

    static int BungeeTargetScore(const VSGameState &state, const VSPlantState &plant, int row) {
        int score = PlantValueScore(plant) + static_cast<int>(plant.position.col) * 8;
        if (IsPlantCombatSeed(plant.seedType)) {
            score += 220;
        }
        score += PlantLaneWeaknessScore(state, row) / 3;
        return score;
    }

    static int CardScore(const VSCardState &card, const VSGameState &state, int targetRow, int economyCount, int effectiveCost) {
        const SeedType seed = static_cast<SeedType>(card.seedType);
        const bool hasPlants = std::any_of(state.plants.begin(), state.plants.end(), [](const VSPlantState &plant) { return !IsDeadOrOutside(plant); });
        const bool hasSnowPea = HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, targetRow);
        const bool hasBonkChoy = HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, targetRow);
        const bool hasWallnut = HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, targetRow) || HasPlantTypeInRow(state, SeedType::SEED_TALLNUT, targetRow);
        const bool hasPumpkinShell = HasPlantTypeInRow(state, SeedType::SEED_PUMPKINSHELL, targetRow);
        const int plantCount = CountPlantsInRow(state, targetRow);
        const int zombieCount = CountZombiesInRow(state, targetRow);
        const int graveProjectileThreat = StraightProjectileThreatScore(state, targetRow);
        const bool hasGraveGuard = HasZombieGraveGuardInRow(state, targetRow);
        const int economyTarget = state.isSuddenDeath ? economyCount : state.rows * 3;
        const int heavyEconomyThreshold = HeavyZombieEconomyThreshold(state);
        const int sustainedOutput = SustainedOutputScoreInRow(state, targetRow);
        const int economyValue = PlantEconomyValueInRow(state, targetRow);
        const PlantLaneAssessment targetLane = AssessPlantLane(state, targetRow);
        const int areaCounterExposure = PlantAreaCounterExposure(state, targetRow);

        int score = 20 + ZombieLaneAttackScore(state, targetRow);
        const int graveThreat = GraveThreatScore(state, targetRow);
        // A grave is the zombie player's income source. Any available
        // pressure is deliberately biased toward a lane that is shooting it.
        score += graveThreat * 2;
        switch (seed) {
            case SeedType::SEED_ZOMBIE_BOBSLED:
                // After the opening graves, the replay's first proactive pressure is Bobsled into a held lane.
                score += 95 + plantCount * 16 + sustainedOutput / 2 + economyValue / 3 + (hasSnowPea ? 190 : 0) + (hasBonkChoy ? 120 : 0);
                break;
            case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
                score += 80 + plantCount * 12 + sustainedOutput / 3 + (hasSnowPea ? 115 : 0) + (hasWallnut ? 80 : 0);
                break;
            case SeedType::SEED_ZOMBIE_PAIL:
                score += 65 + plantCount * 14 + sustainedOutput / 2 + economyValue / 4 + (hasSnowPea ? 135 : 0) + (hasBonkChoy ? 100 : 0);
                break;
            case SeedType::SEED_ZOMBONI:
                // The ice trail makes Zomboni a strong answer to protected,
                // developed lanes, matching the second replay's breakthrough.
                score += 115 + plantCount * 18 + sustainedOutput / 2 + economyValue / 3 + (hasWallnut ? 135 : 0) + (hasSnowPea ? 90 : 0);
                break;
            case SeedType::SEED_ZOMBIE_TRASHCAN:
                // Trashcan is deliberately a slow front-line shield: a
                // single one in the lane blocks pea-family fire before it
                // reaches the graves behind it.
                score += graveProjectileThreat > 0 && !hasGraveGuard ? 425 + graveProjectileThreat * 2 : -110;
                score += graveThreat >= 100 ? 90 : 0;
                break;
            case SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER:
                // Giga Polevaulter is a committed breakthrough card, not an
                // early-game answer. Require an almost complete grave field.
                {
                    const bool hasBreakthroughTarget = plantCount >= 3 || hasWallnut || hasPumpkinShell || sustainedOutput >= 80 || economyValue >= 120;
                    score += economyCount >= heavyEconomyThreshold ? (hasBreakthroughTarget ? 250 : 15) : -240;
                    score += plantCount * 16 + sustainedOutput / 2 + economyValue / 3;
                    score += (hasWallnut ? 145 : 0) + (hasPumpkinShell ? 105 : 0) + (hasSnowPea ? 70 : 0);
                    score += targetLane.defense >= 120 ? 75 : 0;
                    score -= areaCounterExposure / 3;
                    score -= zombieCount >= 2 ? 145 : 0;
                }
                break;
            case SeedType::SEED_ZOMBIE_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_FOOTBALL:
                // Heavy cards are release cards, not automatic reinforcements.
                // A human-like commit seeks a defended economic line to force
                // several answers, and avoids walking a giant into a formed
                // Ash cluster merely because friendly zombies are already there.
                {
                    const bool hasBreakthroughTarget = plantCount >= 3 || hasWallnut || sustainedOutput >= 100 || economyValue >= 150;
                    score += economyCount >= heavyEconomyThreshold ? (hasBreakthroughTarget ? 285 : 35) : -220;
                    score += plantCount * 18 + sustainedOutput / 2 + economyValue / 3;
                    score += (hasWallnut ? 135 : 0) + (hasPumpkinShell ? 110 : 0) + (hasBonkChoy ? 100 : 0) + (hasSnowPea ? 75 : 0);
                    score += targetLane.defense >= 150 ? 90 : 0;
                    score -= areaCounterExposure / 2;
                    score -= zombieCount >= 2 ? 125 : 0;
                }
                break;
            case SeedType::SEED_ZOMBIE_PEA_HEAD:
            case SeedType::SEED_ZOMBIE_NEWSPAPER:
            case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
                score += plantCount * 10 + sustainedOutput / 3 + economyValue / 4 + (hasSnowPea ? 120 : 0);
                break;
            case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
                score += 45 + plantCount * 10 + sustainedOutput / 4 + economyValue / 5;
                score += hasSnowPea ? 70 : 0;
                break;
            case SeedType::SEED_ZOMBIE_LADDER:
                // Ladders are only a worthwhile commitment against an
                // established nut line; otherwise a cheaper probe is better.
                score += hasWallnut ? 275 : -65;
                score += plantCount * 8 + sustainedOutput / 3 + economyValue / 4;
                break;
            case SeedType::SEED_ZOMBIE_SUNDAY_EDITION:
                // The replay uses Sunday Edition as a late, multi-lane
                // pressure card after the grave economy is established.
                score += economyCount >= heavyEconomyThreshold ? 145 : -170;
                score += plantCount * 14 + sustainedOutput / 2 + economyValue / 3;
                score += targetLane.defense >= 120 ? 70 : 0;
                score -= areaCounterExposure / 3;
                break;
            case SeedType::SEED_ZOMBIE_IMP:
            case SeedType::SEED_ZOMBIE_DIGGER:
                score += plantCount * 8 + sustainedOutput / 4 + economyValue / 2 + (hasWallnut ? 90 : 0);
                break;
            case SeedType::SEED_ZOMBIE_BUNGEE:
                score += hasPlants ? 220 : -80;
                score += (hasWallnut || hasBonkChoy) ? 85 : 0;
                break;
            case SeedType::SEED_ZOMBIE_GRAVESTONE:
                if (economyCount < economyTarget) {
                    // Replay construction spans the full three zombie-side
                    // columns. The fixed four-grave opening was too small.
                    score += 450 + (economyTarget - economyCount) * 35;
                } else {
                    score -= 180;
                }
                score += plantCount * 4;
                break;
            case SeedType::SEED_ZOMBIE_MOUND:
                score += economyCount > 0 ? 180 : -150;
                score += graveThreat;
                break;
            case SeedType::SEED_ZOMBIE_DANCER:
                score += 75 + plantCount * 12 + (graveThreat > 0 ? 35 : 0);
                break;
            case SeedType::SEED_ZOMBIE_CATAPULT:
            case SeedType::SEED_ZOMBIE_BALLOON:
                score += 65 + plantCount * 8 + (hasSnowPea ? 75 : 0);
                break;
            default:
                score += plantCount * 7 + sustainedOutput / 4 + economyValue / 4;
                break;
        }
        if (seed != SeedType::SEED_ZOMBIE_TRASHCAN && IsZombieGraveGuardSeed(seed)) {
            // The replay with Screen Door has no Trashcan.  A Door, Pail or
            // Wall-nut Head must still be allowed to screen direct fire from
            // the zombie-side economy instead of treating Trashcan as unique.
            score += graveProjectileThreat > 0 && !hasGraveGuard ? 260 + graveProjectileThreat : -35;
        }
        const bool isEconomyOrTargetedSeed = seed == SeedType::SEED_ZOMBIE_GRAVESTONE || seed == SeedType::SEED_ZOMBIE_MOUND
            || seed == SeedType::SEED_ZOMBIE_BUNGEE;
        const bool isEmergencyGraveGuard = IsZombieGraveGuardSeed(seed) && graveProjectileThreat > 0 && !hasGraveGuard;
        if (zombieCount > 0 && !isEconomyOrTargetedSeed && !IsHeavyZombieSeed(seed) && !isEmergencyGraveGuard) {
            // A cheap/medium zombie is a probe, not a reason to feed the
            // same Ash target.  After one probe, opening another line with
            // Sunflowers is more valuable than reinforcing this line.
            score -= 340 + (zombieCount - 1) * 210;
            score -= areaCounterExposure;
            if (EconomyPlantsInRow(state, targetRow) == 0) {
                score -= 90;
            }
        }
        score += StrategyBonus(state, VSSide::Zombies, seed, targetRow);
        score -= effectiveCost / 50;
        return score;
    }

public:
    void Reset() override {
        BuiltinVSAgent::Reset();
        mLastAttackRow = -1;
    }

    std::optional<VSAction> Decide(const VSGameState &state) override {
        AdvanceBlockedSlots();
        for (const VSResourceState &resource : state.resources) {
            if (resource.side == VSSide::Zombies && !resource.dead && !resource.beingCollected) {
                return VSAction{.side = VSSide::Zombies, .kind = VSActionKind::CollectResource, .objectId = resource.id, .sequence = ++mSequence};
            }
        }

        const int economyCount = CountZombieEconomy(state);
        const int economyTarget = state.rows * 3;
        const int graveDefenseRow = MostThreatenedEconomyRow(state);
        const int graveDefenseScore = GraveThreatScore(state, graveDefenseRow);
        const int graveProjectileThreat = StraightProjectileThreatScore(state, graveDefenseRow);
        const bool hasGraveGuard = HasZombieGraveGuardInRow(state, graveDefenseRow);
        const bool canDeployGraveGuard = graveProjectileThreat > 0 && !hasGraveGuard && HasReadyZombieGraveGuard(state);
        const int activePressureRows = CountActiveZombieRows(state);
        const int survivingFrontRow = MostValuableZombieFrontRow(state);
        const int survivingFrontValue = ZombieFrontlineValueInRow(state, survivingFrontRow);
        const bool preserveSurvivingFront = economyCount >= state.rows && activePressureRows == 1 && survivingFrontValue >= 90;
        const bool survivingFrontGuarded = HasZombieGraveGuardInRow(state, survivingFrontRow);
        const int economicRow = economyCount < state.rows * 2 ? LeastCommittedZombieRow(state) : LeastThreatenedEconomyRow(state);
        if (economyCount < economyTarget && !canDeployGraveGuard && graveDefenseScore < 250 && !preserveSurvivingFront) {
            if (std::optional<VSAction> action = TryBuildEconomy(state, economicRow)) {
                return action;
            }
        }

        const int heavyZombieReserve = HeavyZombieReserve(state);
        const int heavyEconomyThreshold = HeavyZombieEconomyThreshold(state);
        const bool saveForHeavy = heavyZombieReserve > 0 && economyCount >= heavyEconomyThreshold && activePressureRows >= 2
            && state.zombieBrains < heavyZombieReserve && graveDefenseScore < 100;

        auto FindTarget = [&](const VSCardState &card, int row) -> std::optional<VSGridPosition> {
            const SeedType seed = static_cast<SeedType>(card.seedType);
            if (seed == SeedType::SEED_ZOMBIE_GRAVESTONE) {
                const VSGridPosition target = FindZombieEconomyCell(state, row);
                return target.col >= 0 && target.row >= 0 ? std::optional<VSGridPosition>(target) : std::nullopt;
            }
            if (seed == SeedType::SEED_ZOMBIE_MOUND) {
                const VSGridPosition target = FindZombieMoundCell(state, row);
                return target.col >= 0 && target.row >= 0 ? std::optional<VSGridPosition>(target) : std::nullopt;
            }
            if (seed == SeedType::SEED_ZOMBIE_TRASHCAN) {
                // Trashcan advances too slowly to be an attacking probe. Its
                // job is to absorb pea-family fire before it reaches a grave.
                if (StraightProjectileThreatScore(state, row) <= 0 || HasZombieGraveGuardInRow(state, row)) {
                    return std::nullopt;
                }
            }
            if (IsTargetedSeed(card.seedType)) {
                const VSPlantState *targetPlant = nullptr;
                int targetScore = std::numeric_limits<int>::min();
                for (const VSPlantState &plant : state.plants) {
                    if (IsDeadOrOutside(plant) || plant.position.row != row || PlantValueScore(plant) < 100) {
                        continue;
                    }
                    const int plantScore = BungeeTargetScore(state, plant, row);
                    if (targetPlant == nullptr || plantScore > targetScore) {
                        targetPlant = &plant;
                        targetScore = plantScore;
                    }
                }
                return targetPlant == nullptr ? std::nullopt : std::optional<VSGridPosition>(targetPlant->position);
            }
            const VSGridPosition target = FindZombieCell(state, seed, row);
            return target.col >= 0 && target.row >= 0 ? std::optional<VSGridPosition>(target) : std::nullopt;
        };

        const VSCardState *bestCard = nullptr;
        const bool graveDefenseUrgent = graveDefenseScore >= 100;
        int targetRow = graveDefenseUrgent ? graveDefenseRow : MostVulnerablePlantRow(state);
        int bestScore = std::numeric_limits<int>::min();
        for (const VSCardState &card : state.seedBanks[1]) {
            if (IsSlotBlocked(card.slot) || !card.active || card.refreshing || card.refreshCounter > 0) {
                continue;
            }
            if (graveDefenseScore >= 100 && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_ZOMBIE_GRAVESTONE)) {
                continue;
            }
            for (int row = 0; row < state.rows; ++row) {
                const std::optional<VSGridPosition> target = FindTarget(card, row);
                if (!target.has_value() || !IsCardReadyForZombieTarget(card, state, *target)) {
                    continue;
                }
                const int effectiveCost = static_cast<SeedType>(card.seedType) == SeedType::SEED_ZOMBIE_MOUND
                    ? MoundUpgradeCostAt(state, *target)
                    : card.cost;
                int score = CardScore(card, state, row, economyCount, effectiveCost);
                if (graveDefenseUrgent && row == graveDefenseRow) {
                    score += 140;
                }
                if (preserveSurvivingFront && row == survivingFrontRow) {
                    const SeedType seed = static_cast<SeedType>(card.seedType);
                    if (IsZombieGraveGuardSeed(seed) && !survivingFrontGuarded) {
                        // After two attack lanes have been cleared, keep the
                        // remaining valuable front alive before restarting
                        // economic expansion on an empty route.
                        score += 320;
                    } else if (!IsHeavyZombieSeed(seed) && seed != SeedType::SEED_ZOMBIE_GRAVESTONE
                               && seed != SeedType::SEED_ZOMBIE_MOUND) {
                        score += 75;
                    }
                }
                if (saveForHeavy && !IsHeavyZombieSeed(static_cast<SeedType>(card.seedType))) {
                    // Keep the giant plan in mind without freezing the board:
                    // cheap probes remain legal, while medium-cost cards are
                    // less attractive until the heavy card can be afforded.
                    score += card.cost <= std::max(75, heavyZombieReserve / 3) ? 18 : -45;
                }
                if (!graveDefenseUrgent && !preserveSurvivingFront && row == mLastAttackRow) {
                    // Do not keep feeding the same lane while another lane can
                    // accept a zombie. This penalty is intentionally skipped
                    // during urgent grave defense.
                    score -= activePressureRows >= 2 ? 210 : 125;
                }
                if (bestCard == nullptr || score > bestScore) {
                    bestCard = &card;
                    targetRow = row;
                    bestScore = score;
                }
            }
        }
        if (bestCard == nullptr) {
            return std::nullopt;
        }

        const std::optional<VSGridPosition> target = FindTarget(*bestCard, targetRow);
        if (!target.has_value()) {
            return std::nullopt;
        }
        const SeedType chosenSeed = static_cast<SeedType>(bestCard->seedType);
        if (chosenSeed != SeedType::SEED_ZOMBIE_GRAVESTONE && chosenSeed != SeedType::SEED_ZOMBIE_MOUND
            && chosenSeed != SeedType::SEED_ZOMBIE_BUNGEE) {
            mLastAttackRow = targetRow;
        }
        return MakePlayAction(VSSide::Zombies, *bestCard, *target, state.boardTick);
    }
};
} // namespace

std::unique_ptr<IVSAgent> CreateBuiltinVSAgent(VSSide side) {
    switch (side) {
        case VSSide::Plants:
            return std::make_unique<PlantVSAgent>();
        case VSSide::Zombies:
            return std::make_unique<ZombieVSAgent>();
    }
    return nullptr;
}

} // namespace vsai
