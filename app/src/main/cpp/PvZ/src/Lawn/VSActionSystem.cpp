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

#include "PvZ/Lawn/VSActionSystem.h"

#include "PvZ/GlobalVariable.h"
#include "PvZ/Lawn/Board/Board.h"
#include "PvZ/Lawn/Board/Coin.h"
#include "PvZ/Lawn/Board/CursorObject.h"
#include "PvZ/Lawn/Board/GridItem.h"
#include "PvZ/Lawn/Board/Plant.h"
#include "PvZ/Lawn/Board/SeedBank.h"
#include "PvZ/Lawn/Board/SeedPacket.h"
#include "PvZ/Lawn/Board/Zombie.h"
#include "PvZ/Lawn/GamepadControls.h"
#include "PvZ/Lawn/LawnApp.h"
#include "PvZ/Lawn/Widget/VSSetupAddonWidget.h"
#include "PvZ/ReplaySystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

namespace vsai {
namespace {

constexpr std::size_t kSideCount = 2;
constexpr std::size_t kMaxQueuedActions = 64;
constexpr std::uint32_t kDefaultThinkIntervalTicks = 10;

struct QueuedAction {
    VSAction action;
    std::optional<VSSide> sourceSide;
};

struct RuntimeState {
    Board *board = nullptr;
    std::array<std::unique_ptr<IVSAgent>, kSideCount> agents;
    std::array<bool, kSideCount> builtinAgents = {false, false};
    std::array<std::uint32_t, kSideCount> thinkIntervals = {kDefaultThinkIntervalTicks, kDefaultThinkIntervalTicks};
    std::array<std::uint32_t, kSideCount> nextThinkTicks = {0, 0};
    std::deque<QueuedAction> queuedActions;
    bool matchActive = false;
};

RuntimeState gRuntime;

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
        case ZombieType::ZOMBIE_TRASHCAN:
        case ZombieType::ZOMBIE_WALLNUT_HEAD:
        case ZombieType::ZOMBIE_GIGA_FOOTBALL:
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

int LeastDevelopedPlantRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::max();
    for (int row = 0; row < state.rows; ++row) {
        const PlantLaneAssessment assessment = AssessPlantLane(state, row);
        const int sunflowerCount = static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [row](const VSPlantState &plant) {
            const SeedType seed = static_cast<SeedType>(plant.seedType);
            return !IsDeadOrOutside(plant) && plant.position.row == row && (seed == SeedType::SEED_SUNFLOWER || seed == SeedType::SEED_SUNSHROOM);
        }));
        const int score = assessment.defense + assessment.plantCount * 12 + sunflowerCount * 15;
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

bool IsPlantEconomySeed(std::uint16_t seedType) {
    return seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNFLOWER) || seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNSHROOM)
        || seedType == static_cast<std::uint16_t>(SeedType::SEED_TWINSUNFLOWER);
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
        if (IsDeadOrOutside(plant) || plant.position.row != row || !IsPlantEconomySeed(plant.seedType)) {
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
        economyPlants += IsPlantEconomySeed(plant.seedType) ? 1 : 0;
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
    return static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [row](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.position.row == row && IsPlantEconomySeed(plant.seedType);
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
    return card.active && !card.refreshing && card.refreshCounter <= 0 && MoundUpgradeCostAt(state, target) <= state.zombieBrains;
}

int CountZombieEconomy(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.gridItems.begin(), state.gridItems.end(), [](const VSGridItemState &item) {
        return !item.dead && (item.gridItemType == GridItemType::GRIDITEM_GRAVESTONE || item.gridItemType == GridItemType::GRIDITEM_MP_BURIAL_MOUND);
    }));
}

bool IsReadyCard(const VSCardState &card, int resource) {
    return card.seedType != static_cast<std::uint16_t>(SeedType::SEED_NONE) && card.active && !card.refreshing && card.refreshCounter <= 0 && card.cost <= resource;
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

    std::optional<VSAction> TryIncomePlant(const VSGameState &state, int row, int protectedSun) {
        const VSGridPosition target = FindSafeIncomeCell(state, row);
        if (target.col < 0 || target.row < 0) {
            return std::nullopt;
        }
        for (const SeedType seedType : {SeedType::SEED_SUNFLOWER, SeedType::SEED_SUNSHROOM}) {
            if (const VSCardState *card = FindReadyCard(state, seedType); card != nullptr) {
                if (state.plantSun - card->cost < protectedSun) {
                    continue;
                }
                return MakePlayAction(VSSide::Plants, *card, target, state.boardTick);
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
                score += SeedEconomyPressureOpportunity(state, seed, targetRow) * 2;
                score += lane.danger >= 85 ? 55 : 0;
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
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [](const VSCardState &card) {
            return card.seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNFLOWER)
                || card.seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNSHROOM);
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
            if (IsDeadOrOutside(plant) || !plant.asleep) {
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

    std::optional<VSAction> TryPumpkinShell(const VSGameState &state, int row) {
        const VSCardState *card = FindReadyCard(state, SeedType::SEED_PUMPKINSHELL);
        if (card == nullptr) {
            return std::nullopt;
        }

        const VSPlantState *bestPlant = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const VSPlantState &plant : state.plants) {
            if (IsDeadOrOutside(plant) || plant.position.row != row || plant.seedType == static_cast<std::uint16_t>(SeedType::SEED_PUMPKINSHELL)
                || (!IsPlantCombatSeed(plant.seedType) && !IsPlantEconomySeed(plant.seedType))
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
                || seed == SeedType::SEED_INSTANT_COFFEE || seed == SeedType::SEED_PUMPKINSHELL) {
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

            if ((seed == SeedType::SEED_WALLNUT || seed == SeedType::SEED_TALLNUT)
                && (!hasActiveZombie || danger.closest == nullptr || danger.danger < 90)) {
                continue;
            }
            if (seed == SeedType::SEED_CHOMPER && (!hasActiveZombie || danger.closest == nullptr || danger.danger < 90)) {
                continue;
            }

            int row = danger.danger >= 105 ? danger.row : buildRow;
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
        const int incomePlantCount = CountPlantType(state, SeedType::SEED_SUNFLOWER) + CountPlantType(state, SeedType::SEED_SUNSHROOM);
        const int sustainedOutputCount = CountSustainedOutputPlants(state);
        const bool hasIncomeSeed = HasIncomeSeed(state);
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
        const bool impPearThreat = hasGargantuar && counterClosest != nullptr
            && (counterClosest->eating || counterClosest->positionX < 780.0f || counterLane.danger >= 160);
        const int areaCounterReserve = AreaCounterReserve(state);
        const int incomeExpansionTarget = state.rows * 3;
        const bool immediateCounterThreat = squashThreat || impPearThreat;
        const bool mustHoldCounterReserve = areaCounterReserve > 0 && state.plantSun >= areaCounterReserve
            && HasReadyZombieBreakthroughCard(state);
        const int protectedSun = mustHoldCounterReserve ? areaCounterReserve : 0;
        const int zombieEconomyStrikeRow = MostVulnerableZombieEconomyRow(state);
        const int zombieEconomyStrikeValue = ZombieEconomyAttackOpportunity(state, zombieEconomyStrikeRow);
        const bool canStrikeZombieEconomy = zombieEconomyStrikeValue >= 220 && danger.danger < 130 && !immediateCounterThreat;
        // Every two economy plants should fund one durable attacker, up to a
        // line per row.  This prevents the old all-Sunflower opening from
        // leaving the board without enough repeatable damage.
        const int desiredOutputCount = std::min(state.rows, std::max(1, (incomePlantCount + 1) / 2));
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

        if (zombieCluster && areaCounterReserve > 0 && state.plantSun < areaCounterReserve) {
            // Keep the remaining sun for the first available area answer. A
            // cheap shooter or nut cannot solve a multi-zombie pileup as well
            // as the reserved counter card.
            return std::nullopt;
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

        if (danger.danger >= 105) {
            if (!HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_SNOWPEA, danger.row, 1, 2)) {
                    return action;
                }
            }
            if (!HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_BONK_CHOY, danger.row, 3, 3)) {
                    return action;
                }
            }
            if (std::optional<VSAction> action = TryPumpkinShell(state, danger.row)) {
                return action;
            }
            if (hasActiveZombie && danger.closest != nullptr && !HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_WALLNUT, danger.row, 4, 4)) {
                    return action;
                }
            }
        }

        const int buildRow = LeastDevelopedPlantRow(state);
        if (std::optional<VSAction> action = TrySustainedOutputPlant(state, buildRow, protectedSun)) {
            return action;
        }
        if (!HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, buildRow)) {
            if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_BONK_CHOY, buildRow, 3, 3)) {
                return action;
            }
        }
        if (hasActiveZombie && danger.danger >= 90 && !HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, buildRow)) {
            if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_WALLNUT, buildRow, 4, 4)) {
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
            if (IsSlotBlocked(card.slot) || !card.active || card.seedType == static_cast<std::uint16_t>(SeedType::SEED_NONE)) {
                continue;
            }
            if (IsHeavyZombieSeed(static_cast<SeedType>(card.seedType))) {
                reserve = std::min(reserve, std::max(0, card.cost));
            }
        }
        return reserve == std::numeric_limits<int>::max() ? 0 : reserve;
    }

    std::optional<VSAction> TryBuildEconomy(const VSGameState &state, int row) {
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
        const int plantCount = CountPlantsInRow(state, targetRow);
        const int zombieCount = CountZombiesInRow(state, targetRow);
        const int graveProjectileThreat = StraightProjectileThreatScore(state, targetRow);
        const bool hasGraveGuard = HasZombieGraveGuardInRow(state, targetRow);
        const int economyTarget = state.rows * 3;
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
            case SeedType::SEED_ZOMBIE_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_FOOTBALL:
                // Heavy cards are release cards, not automatic reinforcements.
                // A human-like commit seeks a defended economic line to force
                // several answers, and avoids walking a giant into a formed
                // Ash cluster merely because friendly zombies are already there.
                {
                    const bool hasBreakthroughTarget = plantCount >= 3 || hasWallnut || sustainedOutput >= 100 || economyValue >= 150;
                    score += economyCount >= 4 ? (hasBreakthroughTarget ? 285 : 35) : -140;
                    score += plantCount * 18 + sustainedOutput / 2 + economyValue / 3;
                    score += (hasWallnut ? 135 : 0) + (hasBonkChoy ? 100 : 0) + (hasSnowPea ? 75 : 0);
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
        const int economicRow = LeastThreatenedEconomyRow(state);
        if (economyCount < economyTarget && !canDeployGraveGuard && graveDefenseScore < 250) {
            if (std::optional<VSAction> action = TryBuildEconomy(state, economicRow)) {
                return action;
            }
        }

        const int heavyZombieReserve = HeavyZombieReserve(state);
        const int activePressureRows = CountActiveZombieRows(state);
        const bool saveForHeavy = heavyZombieReserve > 0 && economyCount >= 4 && activePressureRows >= 2
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
                if (saveForHeavy && !IsHeavyZombieSeed(static_cast<SeedType>(card.seedType))) {
                    // Keep the giant plan in mind without freezing the board:
                    // cheap probes remain legal, while medium-cost cards are
                    // less attractive until the heavy card can be afforded.
                    score += card.cost <= std::max(75, heavyZombieReserve / 3) ? 18 : -45;
                }
                if (!graveDefenseUrgent && row == mLastAttackRow) {
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

constexpr std::size_t SideIndex(VSSide side) {
    return static_cast<std::size_t>(side);
}

bool IsValidSide(VSSide side) {
    return SideIndex(side) < kSideCount;
}

bool IsTickBefore(std::uint32_t tick, std::uint32_t deadline) {
    return static_cast<std::int32_t>(tick - deadline) < 0;
}

bool IsActionExpired(const VSAction &action, std::uint32_t tick) {
    return action.expiresAtTick != 0 && IsTickBefore(action.expiresAtTick, tick);
}

bool IsActionDeferred(const VSAction &action, std::uint32_t tick) {
    return action.notBeforeTick != 0 && IsTickBefore(tick, action.notBeforeTick);
}

bool IsLocalVSMatch(const Board *board) {
    return board != nullptr && board->mApp != nullptr && board->mApp->IsVSMode() && !IsOnlineModeActive();
}

bool IsMatchPlaying(const Board *board) {
    return board != nullptr && board->mApp != nullptr && board->mApp->mGameScene == GameScenes::SCENE_PLAYING;
}

bool IsMatchPaused(const Board *board) {
    return board != nullptr && (board->mPaused || requestPause);
}

GamepadControls *FindControlsForSide(Board *board, VSSide side) {
    if (board == nullptr) {
        return nullptr;
    }

    const bool wantsZombieControls = side == VSSide::Zombies;
    for (GamepadControls *controls : board->mGamepadControls) {
        if (controls != nullptr && controls->mIsZombie == wantsZombieControls) {
            return controls;
        }
    }
    return nullptr;
}

SeedBank *FindSeedBankForSide(Board *board, VSSide side) {
    if (board == nullptr) {
        return nullptr;
    }

    const bool wantsZombieBank = side == VSSide::Zombies;
    for (SeedBank *seedBank : board->mSeedBank) {
        if (seedBank != nullptr && seedBank->mIsZombie == wantsZombieBank) {
            return seedBank;
        }
    }
    return nullptr;
}

bool IsValidGridTarget(const Board *board, VSGridPosition target) {
    if (board == nullptr) {
        return false;
    }

    const int rowCount = board->StageHas6Rows() ? 6 : 5;
    return target.col >= 0 && target.col < 9 && target.row >= 0 && target.row < rowCount;
}

void SetCursorForSeed(Board *board, GamepadControls *controls, const SeedPacket &packet, std::uint8_t seedSlot, VSGridPosition target) {
    const int gridX = static_cast<int>(target.col);
    const int gridY = static_cast<int>(target.row);
    controls->mCursorPositionX = static_cast<float>(board->GridToPixelX(gridX, gridY) + board->GridCellWidth(gridX, gridY) / 2);
    controls->mCursorPositionY = static_cast<float>(board->GridToPixelY(gridX, gridY) + board->GridCellHeight(gridX, gridY) / 2);
    controls->mGridCenterPositionX = controls->mCursorPositionX;
    controls->mGridCenterPositionY = controls->mCursorPositionY;
    controls->mSelectedSeedIndex = static_cast<int>(seedSlot);
    controls->mSelectedSeedType = packet.mPacketType == SeedType::SEED_IMITATER ? packet.mImitaterType : packet.mPacketType;
    controls->mGamepadState = BaseGamepadControls::MOVEMENT_STATE_PLANT_CURSOR;

    CursorObject *cursor = board->mCursorObject[controls->mPlayerIndex];
    if (cursor != nullptr) {
        cursor->mCursorType = CursorType::CURSOR_TYPE_PLANT_FROM_BANK;
        cursor->mSelectedIndex = static_cast<int>(seedSlot);
        cursor->mType = packet.mPacketType;
        cursor->mImitaterType = packet.mImitaterType;
    }
}

VSActionResult ExecutePlaySeed(Board *board, const VSAction &action) {
    if (!IsValidGridTarget(board, action.target)) {
        return VSActionResult::RejectedInvalidTarget;
    }

    GamepadControls *controls = FindControlsForSide(board, action.side);
    SeedBank *seedBank = FindSeedBankForSide(board, action.side);
    if (controls == nullptr || seedBank == nullptr || controls->GetSeedBank() != seedBank) {
        return VSActionResult::RejectedUnsupported;
    }

    const int packetCount = std::clamp(seedBank->mNumPackets, 0, static_cast<int>(std::size(seedBank->mSeedPackets)));
    if (action.seedSlot >= static_cast<std::uint8_t>(packetCount)) {
        return VSActionResult::RejectedInvalidCard;
    }

    SeedPacket &packet = seedBank->mSeedPackets[action.seedSlot];
    if (action.expectedSeedType != kAnySeedType && action.expectedSeedType != static_cast<std::uint16_t>(packet.mPacketType)) {
        return VSActionResult::RejectedInvalidCard;
    }
    if (!packet.CanPickUp()) {
        return VSActionResult::RejectedCardUnavailable;
    }

    const int gridX = static_cast<int>(action.target.col);
    const int gridY = static_cast<int>(action.target.row);
    if (packet.mPacketType == SeedType::SEED_ZOMBIE_MOUND) {
        // Mound upgrade cost depends on the level at the cursor target.
        SetCursorForSeed(board, controls, packet, action.seedSlot, action.target);
    }
    const int cost = board->GetCurrentPlantCost(packet.mPacketType, SeedType::SEED_NONE);
    if (action.side == VSSide::Plants) {
        if (!board->CanTakeSunMoney(cost, 0)) {
            return VSActionResult::RejectedInsufficientResource;
        }
    } else if (!board->CanTakeDeathMoney(cost)) {
        return VSActionResult::RejectedInsufficientResource;
    }
    if (board->HasLevelAwardDropped() || board->CanPlantAt(gridX, gridY, packet.mPacketType) != PlantingReason::PLANTING_OK) {
        return VSActionResult::RejectedInvalidTarget;
    }

    const int resourceBefore = action.side == VSSide::Plants ? board->mSunMoney1 : board->mDeathMoney;
    SetCursorForSeed(board, controls, packet, action.seedSlot, action.target);
    controls->OnButtonDown(Sexy::GamepadButton::GAMEPAD_BUTTON_A, controls->mPlayerIndex, 0);

    const int resourceAfter = action.side == VSSide::Plants ? board->mSunMoney1 : board->mDeathMoney;
    return resourceBefore != resourceAfter || !packet.CanPickUp() ? VSActionResult::Applied : VSActionResult::RejectedInvalidTarget;
}

VSActionResult ExecuteShovel(Board *board, const VSAction &action) {
    if (action.side != VSSide::Plants) {
        return VSActionResult::RejectedUnsupported;
    }
    if (!IsValidGridTarget(board, action.target)) {
        return VSActionResult::RejectedInvalidTarget;
    }

    const int gridX = static_cast<int>(action.target.col);
    const int gridY = static_cast<int>(action.target.row);
    const int pixelX = board->GridToPixelX(gridX, gridY) + board->GridCellWidth(gridX, gridY) / 2;
    const int pixelY = board->GridToPixelY(gridX, gridY) + board->GridCellHeight(gridX, gridY) / 2;
    Plant *plant = board->ToolHitTest(pixelX, pixelY);
    if (plant == nullptr || plant->mDead) {
        return VSActionResult::RejectedInvalidTarget;
    }

    const SeedType seedType = plant->mSeedType;
    const int plantCol = plant->mPlantCol;
    const int plantRow = plant->mRow;
    board->mApp->PlayFoley(FoleyType::FOLEY_USE_SHOVEL);
    plant->Die();
    if (seedType == SeedType::SEED_CATTAIL && board->GetTopPlantAt(plantCol, plantRow, PlantPriority::TOPPLANT_ONLY_PUMPKIN) != nullptr) {
        board->NewPlant(plantCol, plantRow, SeedType::SEED_LILYPAD, SeedType::SEED_NONE, -1);
    }
    return VSActionResult::Applied;
}

VSActionResult ExecuteFireCobCannon(Board *board, const VSAction &action) {
    if (action.side != VSSide::Plants) {
        return VSActionResult::RejectedUnsupported;
    }
    if (action.objectId == 0 || !IsValidGridTarget(board, action.target)) {
        return VSActionResult::RejectedInvalidTarget;
    }

    Plant *plant = board->mPlants.DataArrayTryToGet(action.objectId);
    if (plant == nullptr || plant->mDead || plant->mSeedType != SeedType::SEED_COBCANNON || plant->mState != PlantState::STATE_COBCANNON_READY) {
        return VSActionResult::RejectedUnsupported;
    }

    const int gridX = static_cast<int>(action.target.col);
    const int gridY = static_cast<int>(action.target.row);
    const int pixelX = board->GridToPixelX(gridX, gridY) + board->GridCellWidth(gridX, gridY) / 2;
    const int pixelY = board->GridToPixelY(gridX, gridY) + board->GridCellHeight(gridX, gridY) / 2;
    plant->CobCannonFire(pixelX, pixelY);
    return plant->mState == PlantState::STATE_COBCANNON_FIRING ? VSActionResult::Applied : VSActionResult::RejectedUnsupported;
}

VSActionResult ExecuteCollectResource(Board *board, const VSAction &action) {
    if (action.objectId == 0) {
        return VSActionResult::RejectedInvalidTarget;
    }

    Coin *coin = board->mCoins.DataArrayTryToGet(action.objectId);
    if (coin == nullptr || coin->mDead || coin->mIsBeingCollected || coin->mCoinMotion == CoinMotion::COIN_MOTION_FROM_NEAR_CURSOR) {
        return VSActionResult::RejectedInvalidTarget;
    }
    const bool isPlantResource = coin->IsSun();
    const bool isZombieResource = coin->IsDeath();
    if ((action.side == VSSide::Plants && !isPlantResource) || (action.side == VSSide::Zombies && !isZombieResource)) {
        return VSActionResult::RejectedUnsupported;
    }

    GamepadControls *controls = FindControlsForSide(board, action.side);
    if (controls == nullptr) {
        return VSActionResult::RejectedUnsupported;
    }
    coin->GamepadCursorOver(controls->mPlayerIndex);
    return coin->mIsBeingCollected || coin->mCoinMotion == CoinMotion::COIN_MOTION_FROM_NEAR_CURSOR ? VSActionResult::Applied : VSActionResult::RejectedInvalidTarget;
}

VSActionResult ExecuteConcede(Board *board, const VSAction &action) {
    if (action.side == VSSide::Plants) {
        board->mApp->SetBoardResult(BoardResult::BOARDRESULT_VS_ZOMBIE_WON);
        board->mApp->mGameScene = GameScenes::SCENE_ZOMBIES_WON;
    } else {
        board->mApp->SetBoardResult(BoardResult::BOARDRESULT_VS_PLANT_WON);
        board->mApp->mGameScene = GameScenes::SCENE_PLANTS_WON;
    }
    return VSActionResult::Applied;
}

VSActionResult ExecuteAction(Board *board, const VSAction &action, bool replayExecution) {
    if (!IsValidSide(action.side)) {
        return VSActionResult::RejectedInvalidSide;
    }
    if (!replayExecution && !IsLocalVSMatch(board)) {
        return VSActionResult::RejectedNotLocalVS;
    }
    if (!IsMatchPlaying(board)) {
        return VSActionResult::RejectedMatchNotPlaying;
    }
    if (IsMatchPaused(board)) {
        return VSActionResult::RejectedMatchPaused;
    }

    const std::uint32_t boardTick = static_cast<std::uint32_t>(board->mMainCounter);
    if (!replayExecution && IsActionExpired(action, boardTick)) {
        return VSActionResult::RejectedStale;
    }
    if (!replayExecution && IsActionDeferred(action, boardTick)) {
        return VSActionResult::Deferred;
    }

    switch (action.kind) {
        case VSActionKind::PlaySeed:
            return ExecutePlaySeed(board, action);
        case VSActionKind::Shovel:
            return ExecuteShovel(board, action);
        case VSActionKind::FireCobCannon:
            return ExecuteFireCobCannon(board, action);
        case VSActionKind::CollectResource:
            return ExecuteCollectResource(board, action);
        case VSActionKind::Concede:
            return ExecuteConcede(board, action);
    }
    return VSActionResult::RejectedUnsupported;
}

VSLocalActionReplayEvent MakeReplayEvent(const VSAction &action) {
    VSLocalActionReplayEvent event{};
    event.type = EventType::EVENT_LOCAL_BOARD_ACTION;
    event.size = static_cast<std::uint8_t>(sizeof(event));
    event.side = static_cast<std::uint8_t>(action.side);
    event.kind = static_cast<std::uint8_t>(action.kind);
    event.seedSlot = action.seedSlot;
    event.expectedSeedType = action.expectedSeedType;
    event.objectId = action.objectId;
    event.col = action.target.col;
    event.row = action.target.row;
    event.sequence = action.sequence;
    event.notBeforeTick = action.notBeforeTick;
    event.expiresAtTick = action.expiresAtTick;
    return event;
}

void RecordAppliedAction(Board *board, const VSAction &action) {
    if (board == nullptr || board->mApp == nullptr || gIsReplayMode) {
        return;
    }

    const VSLocalActionReplayEvent event = MakeReplayEvent(action);
    replay::RecordPacket(ReplayPacketDir::Outbound, reinterpret_cast<const std::byte *>(&event), sizeof(event), static_cast<std::uint32_t>(board->mApp->mAppCounter));
}

void Notify(IVSAgent *agent, const VSAction &action, VSActionResult result) {
    if (agent != nullptr) {
        agent->OnActionResult(action, result);
    }
}

void NotifySide(std::optional<VSSide> side, const VSAction &action, VSActionResult result) {
    if (side.has_value()) {
        Notify(GetAgent(*side), action, result);
    }
}

void ResetForBoard(Board *board) {
    if (gRuntime.board == board) {
        return;
    }

    gRuntime.board = board;
    gRuntime.queuedActions.clear();
    gRuntime.nextThinkTicks = {0, 0};
    gRuntime.matchActive = false;
    for (const std::unique_ptr<IVSAgent> &agent : gRuntime.agents) {
        if (agent != nullptr) {
            agent->Reset();
        }
    }
}

void ExecuteQueuedAction(Board *board, const QueuedAction &queuedAction) {
    if (queuedAction.sourceSide.has_value() && !IsSideEnabled(*queuedAction.sourceSide)) {
        NotifySide(queuedAction.sourceSide, queuedAction.action, VSActionResult::RejectedDisabled);
        return;
    }
    const VSActionResult result = ExecuteAction(board, queuedAction.action, false);
    if (result == VSActionResult::Applied) {
        RecordAppliedAction(board, queuedAction.action);
    }
    NotifySide(queuedAction.sourceSide, queuedAction.action, result);
}

void RunAgent(Board *board, VSSide side, const VSGameState &state) {
    const std::size_t index = SideIndex(side);
    IVSAgent *agent = gRuntime.agents[index].get();
    if (agent == nullptr || !IsSideEnabled(side)) {
        return;
    }

    const std::uint32_t tick = state.boardTick;
    if (IsTickBefore(tick, gRuntime.nextThinkTicks[index])) {
        return;
    }
    gRuntime.nextThinkTicks[index] = tick + gRuntime.thinkIntervals[index];

    std::optional<VSAction> action = agent->Decide(state);
    if (!action.has_value()) {
        return;
    }
    if (action->side != side) {
        Notify(agent, *action, VSActionResult::RejectedInvalidSide);
        return;
    }
    if (IsActionExpired(*action, tick)) {
        Notify(agent, *action, VSActionResult::RejectedStale);
        return;
    }
    if (IsActionDeferred(*action, tick)) {
        if (gRuntime.queuedActions.size() >= kMaxQueuedActions) {
            Notify(agent, *action, VSActionResult::RejectedUnsupported);
            return;
        }
        gRuntime.queuedActions.push_back({*action, side});
        Notify(agent, *action, VSActionResult::Queued);
        return;
    }

    const VSActionResult result = ExecuteAction(board, *action, false);
    if (result == VSActionResult::Applied) {
        RecordAppliedAction(board, *action);
    }
    Notify(agent, *action, result);
}

void SyncBuiltinAgents() {
    const std::array<bool, kSideCount> enabled = {IsSideEnabled(VSSide::Plants), IsSideEnabled(VSSide::Zombies)};
    const std::size_t plantIndex = SideIndex(VSSide::Plants);
    const std::size_t zombieIndex = SideIndex(VSSide::Zombies);
    if (enabled[plantIndex] && gRuntime.agents[plantIndex] == nullptr) {
        gRuntime.agents[plantIndex] = std::make_unique<PlantVSAgent>();
        gRuntime.builtinAgents[plantIndex] = true;
    } else if (!enabled[plantIndex] && gRuntime.builtinAgents[plantIndex]) {
        gRuntime.agents[plantIndex].reset();
        gRuntime.builtinAgents[plantIndex] = false;
    }
    if (enabled[zombieIndex] && gRuntime.agents[zombieIndex] == nullptr) {
        gRuntime.agents[zombieIndex] = std::make_unique<ZombieVSAgent>();
        gRuntime.builtinAgents[zombieIndex] = true;
    } else if (!enabled[zombieIndex] && gRuntime.builtinAgents[zombieIndex]) {
        gRuntime.agents[zombieIndex].reset();
        gRuntime.builtinAgents[zombieIndex] = false;
    }
}

} // namespace

void SetAgent(VSSide side, std::unique_ptr<IVSAgent> agent) {
    if (!IsValidSide(side)) {
        return;
    }

    const std::size_t index = SideIndex(side);
    std::erase_if(gRuntime.queuedActions, [side](const QueuedAction &queuedAction) {
        return queuedAction.sourceSide == side;
    });
    gRuntime.agents[index] = std::move(agent);
    gRuntime.builtinAgents[index] = false;
    gRuntime.nextThinkTicks[index] = 0;
    if (gRuntime.agents[index] != nullptr) {
        gRuntime.agents[index]->Reset();
    }
}

void ClearAgent(VSSide side) {
    SetAgent(side, nullptr);
}

IVSAgent *GetAgent(VSSide side) {
    return IsValidSide(side) ? gRuntime.agents[SideIndex(side)].get() : nullptr;
}

void SetThinkIntervalTicks(VSSide side, std::uint32_t ticks) {
    if (!IsValidSide(side)) {
        return;
    }
    gRuntime.thinkIntervals[SideIndex(side)] = std::max(ticks, std::uint32_t{1});
}

std::uint32_t GetThinkIntervalTicks(VSSide side) {
    return IsValidSide(side) ? gRuntime.thinkIntervals[SideIndex(side)] : 0;
}

bool IsSideEnabled(VSSide side) {
    switch (side) {
        case VSSide::Plants:
            return VSSetupAddonWidget::msPlantAIMode;
        case VSSide::Zombies:
            return VSSetupAddonWidget::msZombieAIMode;
    }
    return false;
}

VSGameState BuildGameState(Board *board) {
    VSGameState state{};
    if (board == nullptr) {
        return state;
    }

    state.boardTick = static_cast<std::uint32_t>(board->mMainCounter);
    state.rows = board->StageHas6Rows() ? 6 : 5;
    state.plantSun = board->mSunMoney1;
    state.zombieBrains = board->mDeathMoney;
    state.playing = IsMatchPlaying(board);
    state.paused = board->mPaused || requestPause;

    for (SeedBank *seedBank : board->mSeedBank) {
        if (seedBank == nullptr) {
            continue;
        }

        std::vector<VSCardState> &cards = state.seedBanks[seedBank->mIsZombie ? SideIndex(VSSide::Zombies) : SideIndex(VSSide::Plants)];
        const int packetCount = std::clamp(seedBank->mNumPackets, 0, static_cast<int>(std::size(seedBank->mSeedPackets)));
        cards.reserve(static_cast<std::size_t>(packetCount));
        for (int slot = 0; slot < packetCount; ++slot) {
            const SeedPacket &packet = seedBank->mSeedPackets[slot];
            cards.push_back({
                .slot = static_cast<std::uint8_t>(slot),
                .seedType = static_cast<std::uint16_t>(packet.mPacketType),
                .imitaterType = static_cast<std::uint16_t>(packet.mImitaterType),
                .cost = board->GetCurrentPlantCost(packet.mPacketType, SeedType::SEED_NONE),
                .refreshCounter = packet.mRefreshCounter,
                .refreshTime = packet.mRefreshTime,
                .active = packet.mActive,
                .refreshing = packet.mRefreshing,
            });
        }
    }

    for (Plant *plant = nullptr; board->mPlants.IterateNext(plant);) {
        state.plants.push_back({
            .id = board->mPlants.DataArrayGetID(plant),
            .seedType = static_cast<std::uint16_t>(plant->mSeedType),
            .state = static_cast<std::uint16_t>(plant->mState),
            .position = {static_cast<std::int8_t>(plant->mPlantCol), static_cast<std::int8_t>(plant->mRow)},
            .health = plant->mPlantHealth,
            .maxHealth = plant->mPlantMaxHealth,
            .asleep = plant->mIsAsleep,
            .dead = plant->mDead,
        });
    }

    for (Zombie *zombie = nullptr; board->mZombies.IterateNext(zombie);) {
        state.zombies.push_back({
            .id = board->mZombies.DataArrayGetID(zombie),
            .zombieType = static_cast<std::uint16_t>(zombie->mZombieType),
            .row = static_cast<std::int8_t>(zombie->mRow),
            .positionX = zombie->mPosX,
            .positionY = zombie->mPosY,
            .bodyHealth = zombie->mBodyHealth,
            .bodyMaxHealth = zombie->mBodyMaxHealth,
            .shieldHealth = zombie->mShieldHealth,
            .eating = zombie->mIsEating,
            .dead = zombie->mDead,
        });
    }

    for (GridItem *gridItem = nullptr; board->mGridItems.IterateNext(gridItem);) {
        state.gridItems.push_back({
            .id = board->mGridItems.DataArrayGetID(gridItem),
            .gridItemType = static_cast<std::uint16_t>(gridItem->mGridItemType),
            .position = {static_cast<std::int8_t>(gridItem->mGridX), static_cast<std::int8_t>(gridItem->mGridY)},
            .health = gridItem->mVSGraveStoneHealth,
            .level = gridItem->mMoundLevel,
            .dead = gridItem->mDead,
        });
    }

    for (Coin *coin = nullptr; board->mCoins.IterateNext(coin);) {
        const bool isPlantResource = coin->IsSun();
        const bool isZombieResource = coin->IsDeath();
        if (!isPlantResource && !isZombieResource) {
            continue;
        }
        state.resources.push_back({
            .id = board->mCoins.DataArrayGetID(coin),
            .side = isPlantResource ? VSSide::Plants : VSSide::Zombies,
            .coinType = static_cast<std::uint16_t>(coin->mType),
            .value = coin->GetSunValue(),
            .positionX = coin->mPosX,
            .positionY = coin->mPosY,
            .beingCollected = coin->mIsBeingCollected || coin->mCoinMotion == CoinMotion::COIN_MOTION_FROM_NEAR_CURSOR,
            .dead = coin->mDead,
        });
    }

    return state;
}

bool EnqueueAction(const VSAction &action) {
    if (!IsValidSide(action.side) || gRuntime.queuedActions.size() >= kMaxQueuedActions) {
        return false;
    }
    gRuntime.queuedActions.push_back({action, std::nullopt});
    return true;
}

VSActionResult ExecuteActionNow(Board *board, const VSAction &action) {
    const VSActionResult result = ExecuteAction(board, action, false);
    if (result == VSActionResult::Applied) {
        RecordAppliedAction(board, action);
    }
    return result;
}

void Update(Board *board) {
    ResetForBoard(board);
    SyncBuiltinAgents();
    if (!IsLocalVSMatch(board) || !IsMatchPlaying(board)) {
        if (gRuntime.matchActive) {
            gRuntime.queuedActions.clear();
            gRuntime.nextThinkTicks = {0, 0};
            for (const std::unique_ptr<IVSAgent> &agent : gRuntime.agents) {
                if (agent != nullptr) {
                    agent->Reset();
                }
            }
            gRuntime.matchActive = false;
        }
        return;
    }

    if (!gRuntime.matchActive) {
        gRuntime.queuedActions.clear();
        gRuntime.nextThinkTicks = {0, 0};
        for (const std::unique_ptr<IVSAgent> &agent : gRuntime.agents) {
            if (agent != nullptr) {
                agent->Reset();
            }
        }
        gRuntime.matchActive = true;
    }

    // Pause freezes the board simulation, so AI actions and deferred queues
    // must remain untouched until the same board resumes.
    if (IsMatchPaused(board)) {
        return;
    }

    const std::uint32_t tick = static_cast<std::uint32_t>(board->mMainCounter);
    std::optional<VSSide> actionProcessedForSide;
    for (auto iterator = gRuntime.queuedActions.begin(); iterator != gRuntime.queuedActions.end();) {
        if (IsActionExpired(iterator->action, tick)) {
            NotifySide(iterator->sourceSide, iterator->action, VSActionResult::RejectedStale);
            iterator = gRuntime.queuedActions.erase(iterator);
            continue;
        }
        if (!IsActionDeferred(iterator->action, tick)) {
            const QueuedAction queuedAction = *iterator;
            gRuntime.queuedActions.erase(iterator);
            ExecuteQueuedAction(board, queuedAction);
            actionProcessedForSide = queuedAction.action.side;
            break;
        }
        ++iterator;
    }

    if (actionProcessedForSide != VSSide::Plants) {
        RunAgent(board, VSSide::Plants, BuildGameState(board));
    }
    if (IsMatchPlaying(board) && actionProcessedForSide != VSSide::Zombies) {
        RunAgent(board, VSSide::Zombies, BuildGameState(board));
    }
}

void Reset() {
    gRuntime.board = nullptr;
    gRuntime.queuedActions.clear();
    gRuntime.nextThinkTicks = {0, 0};
    gRuntime.matchActive = false;
    for (const std::unique_ptr<IVSAgent> &agent : gRuntime.agents) {
        if (agent != nullptr) {
            agent->Reset();
        }
    }
}

void ExecuteReplayAction(Board *board, const VSLocalActionReplayEvent &event) {
    if (event.size != sizeof(VSLocalActionReplayEvent)) {
        return;
    }

    const VSAction action{
        .side = static_cast<VSSide>(event.side),
        .kind = static_cast<VSActionKind>(event.kind),
        .seedSlot = event.seedSlot,
        .expectedSeedType = event.expectedSeedType,
        .objectId = event.objectId,
        .target = {event.col, event.row},
        .notBeforeTick = event.notBeforeTick,
        .expiresAtTick = event.expiresAtTick,
        .sequence = event.sequence,
    };
    ExecuteAction(board, action, true);
}

} // namespace vsai
