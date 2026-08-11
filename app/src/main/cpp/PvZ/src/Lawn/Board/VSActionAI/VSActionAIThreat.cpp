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

#include "VSActionAIThreat.h"

#include "VSActionAIStrategy.h"

#include "PvZ/Lawn/Board/Plant.h"
#include "PvZ/Lawn/Common/GameConstants.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vsai::detail {

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

const VSZombieState *FindClosestZombie(const VSGameState &state, int row) {
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
        case ZombieType::ZOMBIE_SUPER_FAN_IMP:
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

bool HasMindControlledZombieInRow(const VSGameState &state, int row) {
    return std::any_of(state.zombies.begin(), state.zombies.end(), [row](const VSZombieState &zombie) {
        return !zombie.dead && zombie.row == row && zombie.mindControlled;
    });
}

bool IsMowerInMotion(const VSGameState &state, int row) {
    return row >= 0 && row < state.rows && row < static_cast<int>(state.mowerInMotion.size())
        && state.mowerInMotion[static_cast<std::size_t>(row)];
}

bool IsMowerAboutToTrigger(const VSGameState &state, int row) {
    if (row < 0 || row >= state.rows || row >= static_cast<int>(state.mowerAvailable.size())
        || !state.mowerAvailable[static_cast<std::size_t>(row)]) {
        return false;
    }

    // Do not feed a fresh zombie into a lane once an invader has entered the
    // first plant column. The ready mower will clear the whole lane shortly.
    constexpr float kMowerTriggerApproachX = static_cast<float>(LAWN_XMIN + 80);
    return std::any_of(state.zombies.begin(), state.zombies.end(), [row](const VSZombieState &zombie) {
        return !zombie.dead && !zombie.mindControlled && zombie.row == row && zombie.positionX <= kMowerTriggerApproachX;
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

bool IsMowerlessThirdColumnEmergency(const VSGameState &state, int row) {
    if (row < 0 || row >= state.rows || row >= static_cast<int>(state.mowerAvailable.size())
        || state.mowerAvailable[static_cast<std::size_t>(row)] || IsMowerInMotion(state, row)) {
        return false;
    }

    // Plant columns are zero based.  Once a zombie crosses the right edge of
    // column two, it has reached the third plant column with no mower left.
    constexpr float kThirdColumnBoundary = static_cast<float>(LAWN_XMIN + 3 * 80);
    return std::any_of(state.zombies.begin(), state.zombies.end(), [row](const VSZombieState &zombie) {
        return !zombie.dead && zombie.row == row && zombie.positionX < kThirdColumnBoundary;
    });
}

bool IsMowerlessStrongPlantLane(const VSGameState &state, int row) {
    if (row < 0 || row >= state.rows || row >= static_cast<int>(state.mowerAvailable.size())
        || state.mowerAvailable[static_cast<std::size_t>(row)] || IsMowerInMotion(state, row) || CountPlantsInRow(state, row) == 0) {
        return false;
    }

    const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
    // An empty mowerless lane is already a poor place to spend a grave. If
    // attackers are present, only suppress the lane when the existing DPS can
    // actually hold it; a genuine deficit still deserves a breakthrough.
    return CountZombiesInRow(state, row) == 0 || firepower.canHold
        || (firepower.dps >= 45 && firepower.deficit <= 20);
}

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
    score += LargestZombieStackInRow(state, row) * 90;
    // Losing a mower turns an intruder in column three into a board-loss
    // risk.  This must outrank every economy and cross-lane opportunity.
    score += IsMowerlessThirdColumnEmergency(state, row) ? 2000 : 0;
    return score;
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
        case ZombieType::ZOMBIE_SQUASH_HEAD:
            return 80;
        case ZombieType::ZOMBIE_PAIL:
        case ZombieType::ZOMBIE_DIGGER:
        case ZombieType::ZOMBIE_POLEVAULTER:
        case ZombieType::ZOMBIE_SUPER_FAN_IMP:
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
        case SeedType::SEED_BLOOMERANG:
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

int PlantDamagePerSecond(SeedType seedType) {
    // These values mirror the relative damage/cadence of the VS plants.  The
    // agent needs a stable tactical estimate rather than animation-perfect
    // frame prediction, so values are rounded to whole damage per second.
    switch (seedType) {
        case SeedType::SEED_GATLINGPEA:
            return 56;
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_WINTERMELON:
            return 48;
        case SeedType::SEED_REPEATER:
            return 28;
        case SeedType::SEED_FUMESHROOM:
            return 24;
        case SeedType::SEED_BLOOMERANG:
            return 22;
        case SeedType::SEED_GLOOMSHROOM:
            return 45;
        case SeedType::SEED_CABBAGEPULT:
            return 26;
        case SeedType::SEED_KERNELPULT:
            return 17;
        case SeedType::SEED_SNOWPEA:
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_THREEPEATER:
            return 14;
        case SeedType::SEED_PUFFSHROOM:
            return 10;
        case SeedType::SEED_SCAREDYSHROOM:
            return 16;
        case SeedType::SEED_STARFRUIT:
            return 18;
        case SeedType::SEED_SPORESHROOM:
            return 16;
        case SeedType::SEED_BONK_CHOY:
        case SeedType::SEED_CELERY_STALKER:
            return 22;
        case SeedType::SEED_CHOMPER:
            return 30;
        default:
            return 0;
    }
}

PlantLaneFirepower AssessPlantLaneFirepower(const VSGameState &state, int row) {
    PlantLaneFirepower assessment{};
    assessment.row = row;
    if (row < 0 || row >= state.rows) {
        return assessment;
    }

    float closestX = std::numeric_limits<float>::max();
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        const int health = std::max(0, zombie.bodyHealth) + std::max(0, zombie.shieldHealth);
        assessment.incomingHealth += health;
        if (zombie.positionX <= 700.0f || zombie.eating) {
            assessment.nearHealth += health;
        }
        closestX = std::min(closestX, zombie.positionX);
    }
    if (closestX == std::numeric_limits<float>::max()) {
        return assessment;
    }

    assessment.closestDistance = std::max(0, static_cast<int>(closestX));
    // A row's actual forward-most plant is the meaningful contact point.
    // Treating every lane as if contact began at a fixed rear coordinate
    // makes a zombie in column five look harmless while it is already about
    // to chew a forward income plant or a defensive line.
    int frontPlantColumn = -1;
    for (const VSPlantState &plant : state.plants) {
        if (!IsDeadOrOutside(plant) && plant.position.row == row) {
            frontPlantColumn = std::max(frontPlantColumn, static_cast<int>(plant.position.col));
        }
    }
    const int contactX = frontPlantColumn < 0
        ? LAWN_XMIN + 120
        : LAWN_XMIN + frontPlantColumn * 80 + 40;
    assessment.secondsToContact = std::clamp((assessment.closestDistance - contactX) / 42, 1, 16);
    for (const VSPlantState &plant : state.plants) {
        if (IsDeadOrOutside(plant) || plant.position.row != row) {
            continue;
        }
        switch (static_cast<SeedType>(plant.seedType)) {
            case SeedType::SEED_WALLNUT:
            case SeedType::SEED_TALLNUT:
                assessment.secondsToContact += 7;
                break;
            case SeedType::SEED_PUMPKINSHELL:
                assessment.secondsToContact += 4;
                break;
            default:
                break;
        }
    }
    assessment.secondsToContact = std::min(24, assessment.secondsToContact);

    for (const VSPlantState &plant : state.plants) {
        if (IsDeadOrOutside(plant) || plant.asleep) {
            continue;
        }
        const SeedType seed = static_cast<SeedType>(plant.seedType);
        const int baseDps = PlantDamagePerSecond(seed);
        if (baseDps == 0) {
            continue;
        }

        const int rowDistance = std::abs(static_cast<int>(plant.position.row) - row);
        int contribution = 0;
        if (plant.position.row == row) {
            contribution = baseDps;
            // Melee output becomes real only after an intruder reaches its
            // attack zone.  It cannot be used to justify a distant lane.
            if ((seed == SeedType::SEED_BONK_CHOY || seed == SeedType::SEED_CELERY_STALKER) && closestX > 560.0f) {
                contribution = 0;
            } else if (seed == SeedType::SEED_CHOMPER && closestX > 500.0f) {
                contribution = 0;
            }
        } else if (seed == SeedType::SEED_THREEPEATER && rowDistance == 1) {
            contribution = baseDps;
        } else if (seed == SeedType::SEED_STARFRUIT && rowDistance == 1) {
            contribution = baseDps * 2 / 3;
        } else if (seed == SeedType::SEED_GLOOMSHROOM && rowDistance == 1 && closestX < 480.0f) {
            contribution = baseDps / 2;
        }
        if (contribution == 0) {
            continue;
        }

        const int healthRatio = plant.maxHealth > 0 ? std::clamp(plant.health * 100 / plant.maxHealth, 0, 100) : 50;
        assessment.dps += contribution * healthRatio / 100;
    }

    // Distant units can still be addressed after the nearest contact; near
    // health is the immediate requirement that decides whether a new shooter
    // is needed before another economy plant.
    const int requiredHealth = assessment.nearHealth > 0 ? assessment.nearHealth : assessment.incomingHealth;
    assessment.damageBeforeContact = assessment.dps * assessment.secondsToContact;
    const int requiredDps = (requiredHealth + assessment.secondsToContact - 1) / assessment.secondsToContact;
    assessment.deficit = std::max(0, requiredDps - assessment.dps);
    assessment.canHold = requiredHealth == 0 || assessment.damageBeforeContact >= requiredHealth;
    return assessment;
}

PlantLaneAssessment AssessPlantLane(const VSGameState &state, int row) {
    PlantLaneAssessment assessment{};
    assessment.row = row;
    assessment.closest = FindClosestZombie(state, row);
    int frontPlantColumn = -1;
    for (const VSPlantState &plant : state.plants) {
        if (!IsDeadOrOutside(plant) && plant.position.row == row) {
            frontPlantColumn = std::max(frontPlantColumn, static_cast<int>(plant.position.col));
        }
    }
    const float frontPlantX = frontPlantColumn < 0
        ? static_cast<float>(LAWN_XMIN + 6 * 80)
        : static_cast<float>(LAWN_XMIN + frontPlantColumn * 80 + 40);
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        const int advance = std::clamp((850 - static_cast<int>(zombie.positionX)) / 3, 0, 240);
        assessment.rawDanger += ZombieThreatWeight(zombie.zombieType) + advance;
        assessment.rawDanger += zombie.eating ? 135 : 0;
        assessment.rawDanger += zombie.positionX < 680.0f ? 35 : 0;
        assessment.rawDanger += zombie.positionX < 600.0f ? 65 : 0;
        assessment.rawDanger += zombie.positionX < 520.0f ? 100 : 0;
        assessment.rawDanger += zombie.positionX < 400.0f ? 160 : 0;
        // Crossing the actual forward plant is a tactical break point. The
        // resulting score outranks economy opportunities in other rows.
        assessment.rawDanger += zombie.positionX <= frontPlantX + 30.0f ? 140 : 0;
        assessment.rawDanger += zombie.positionX <= frontPlantX - 50.0f ? 220 : 0;
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
    const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
    // Unit counts alone hide the difference between a bucket zombie far away
    // and one that reaches a sunflower before the available DPS can remove
    // it.  Surface that shortfall to all existing lane-choice callers.
    assessment.rawDanger += std::min(170, firepower.deficit * 5);
    if (!firepower.canHold && firepower.nearHealth > 0) {
        assessment.rawDanger += 35;
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
        case SeedType::SEED_BLOOMERANG:
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

bool IsPlantProtectedByUmbrella(const VSGameState &state, VSGridPosition position) {
    if (position.col < 0 || position.row < 0) {
        return false;
    }
    return std::any_of(state.plants.begin(), state.plants.end(), [position](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.seedType == static_cast<std::uint16_t>(SeedType::SEED_UMBRELLA)
            && std::abs(static_cast<int>(plant.position.col) - static_cast<int>(position.col)) <= 1
            && std::abs(static_cast<int>(plant.position.row) - static_cast<int>(position.row)) <= 1;
    });
}

bool IsPlantEconomySeed(const VSGameState &state, std::uint16_t seedType) {
    return seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNFLOWER)
        || seedType == static_cast<std::uint16_t>(SeedType::SEED_TWINSUNFLOWER)
        || (state.isNight && seedType == static_cast<std::uint16_t>(SeedType::SEED_SUNSHROOM));
}

bool IsPlantCombatSeed(std::uint16_t seedType) {
    switch (static_cast<SeedType>(seedType)) {
        case SeedType::SEED_SNOWPEA:
        case SeedType::SEED_PUFFSHROOM:
        case SeedType::SEED_SCAREDYSHROOM:
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
        case SeedType::SEED_BLOOMERANG:
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_WINTERMELON:
        case SeedType::SEED_GATLINGPEA:
        case SeedType::SEED_COBCANNON:
        case SeedType::SEED_SPIKEWEED:
        case SeedType::SEED_SPIKEROCK:
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
        case SeedType::SEED_BLOOMERANG:
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
        case SeedType::SEED_BLOOMERANG:
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
        case SeedType::SEED_BLOOMERANG:
            return 135;
        case SeedType::SEED_SNOWPEA:
            return 150;
        case SeedType::SEED_SCAREDYSHROOM:
            return 135;
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
    const SeedType seed = static_cast<SeedType>(plant.seedType);
    if (seed == SeedType::SEED_STARFRUIT) {
        // Starfruit fires in five directions and is the one plant that can
        // threaten a grave from an adjacent row as well as its own row.
        return rowDistance == 0 ? 145 : (rowDistance == 1 ? 75 : 0);
    }
    if (seed == SeedType::SEED_GRAVEBUSTER) {
        return rowDistance == 0 && plant.position.col == economy.position.col ? 250 : 0;
    }
    if (const int projectileThreat = StraightProjectileThreatToEconomy(plant, economy); projectileThreat > 0) {
        return projectileThreat;
    }
    // Pults and mushrooms can lock onto VS graves. Keep this list explicit so
    // melee plants (Bonk Choy, Celery Stalker, Chomper) never inflate grave
    // threat when they cannot reach the zombie economy.
    if (rowDistance != 0 || plant.position.col >= economy.position.col) {
        return 0;
    }
    switch (seed) {
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
        case SeedType::SEED_SPORESHROOM:
            return 75;
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_BLOOMERANG:
            return 90;
        case SeedType::SEED_MELONPULT:
            return 115;
        case SeedType::SEED_WINTERMELON:
            return 135;
        case SeedType::SEED_GLOOMSHROOM:
            return 100;
        default:
            return 0;
    }
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

int LobbedProjectileThreatScore(const VSGameState &state, int row) {
    int score = 0;
    for (const VSGridItemState &item : state.gridItems) {
        if (item.dead || !IsZombieEconomyItem(item.gridItemType) || item.position.row != row) {
            continue;
        }
        for (const VSPlantState &plant : state.plants) {
            if (IsDeadOrOutside(plant) || plant.position.row != row) {
                continue;
            }
            const SeedType seed = static_cast<SeedType>(plant.seedType);
            switch (seed) {
                case SeedType::SEED_CABBAGEPULT:
                case SeedType::SEED_KERNELPULT:
                case SeedType::SEED_MELONPULT:
                case SeedType::SEED_WINTERMELON:
                    score += PlantThreatToEconomy(plant, item);
                    break;
                default:
                    break;
            }
        }
    }
    return score;
}

bool NeedsProactiveGraveScreen(const VSGameState &state, int row) {
    if (row < 0 || row >= state.rows || HasZombieGraveGuardInRow(state, row)) {
        return false;
    }

    int economyAssets = 0;
    for (const VSGridItemState &item : state.gridItems) {
        if (!item.dead && item.position.row == row && IsZombieEconomyItem(item.gridItemType)) {
            ++economyAssets;
        }
    }
    // A live straight shooter has already acquired the rear economic lane.
    // Do not wait for the first tombstone to be half dead before assigning a
    // Trashcan, Door, Pail, or head as the screen.
    const int directThreat = StraightProjectileThreatScore(state, row);
    return economyAssets > 0 && directThreat >= (economyAssets >= 2 ? 55 : 75);
}

int ZombieGraveScreenDeficit(const VSGameState &state, int row) {
    const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
    if (firepower.dps <= 0 || CountZombiesInRow(state, row) == 0) {
        return 0;
    }

    int screenHealth = 0;
    for (const VSZombieState &zombie : state.zombies) {
        if (!zombie.dead && zombie.row == row) {
            screenHealth += std::max(0, zombie.bodyHealth) + std::max(0, zombie.shieldHealth);
        }
    }
    const int horizon = std::max(5, firepower.secondsToContact + 3);
    return std::max(0, firepower.dps * horizon - screenHealth);
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

int ProtectableGraveThreatScore(const VSGameState &state, int row) {
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
            // Gravebuster is already consuming this exact grave. Placing a
            // slow screen in front cannot save it, so it must not bait a
            // Trashcan or Door away from a real projectile threat.
            if (!IsDeadOrOutside(plant) && plant.seedType != static_cast<std::uint16_t>(SeedType::SEED_GRAVEBUSTER)) {
                score += PlantThreatToEconomy(plant, item);
            }
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
    const auto IsLiveZombieTargetRow = [&state](int row) {
        return std::any_of(state.gridItems.begin(), state.gridItems.end(), [row](const VSGridItemState &item) {
            return !item.dead && item.health > 0 && item.position.row == row
                && item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_TARGET_ZOMBIE);
        });
    };
    const bool hasZombieTargets = std::any_of(state.gridItems.begin(), state.gridItems.end(), [](const VSGridItemState &item) {
        return item.gridItemType == static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_TARGET_ZOMBIE);
    });
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        // A lost target route cannot win the game back. On ordinary VS boards
        // keep every defensive decision on a surviving target row instead of
        // paying for a grave screen in a route that is already gone.
        if (hasZombieTargets && !IsLiveZombieTargetRow(row)) {
            continue;
        }
        // Pick the economic row which can still be screened, not merely the
        // one with the largest amount of historical damage.  Direct shooters
        // and pults must both pull a guard toward their current firing lane.
        const int protectableThreat = ProtectableGraveThreatScore(state, row);
        const int straightThreat = StraightProjectileThreatScore(state, row);
        const int lobbedThreat = LobbedProjectileThreatScore(state, row);
        const int screenDeficit = ZombieGraveScreenDeficit(state, row);
        int score = protectableThreat * 2 + straightThreat + lobbedThreat + screenDeficit;
        if (hasZombieTargets) {
            const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
            // Target survival is decided by the firing lane rather than only
            // the health of a grave that happens to be in front of it. Favor
            // high plant DPS, a developed output line, and an unscreened
            // target route so the first guard appears before a third target
            // can be lost.
            score += firepower.dps * 4 + SustainedOutputScoreInRow(state, row) * 3 + screenDeficit * 3;
            score += HasZombieGraveGuardInRow(state, row) ? 0 : 180;
            score += CountZombiesInRow(state, row) == 0 ? 90 : 0;
        }
        if (NeedsProactiveGraveScreen(state, row)) {
            score += 160;
        }
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
    const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
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
    // This lane score feeds the zombie chooser.  Prefer a sunflower row
    // whose actual output cannot clear a current push over a visually sparse
    // row that already has sufficient DPS.
    score += std::max(0, 34 - firepower.dps) * 3;
    score += firepower.deficit * 6;
    score += !firepower.canHold && firepower.nearHealth > 0 ? 95 : 0;
    return score;
}

int EconomyPlantsInRow(const VSGameState &state, int row) {
    return static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [&state, row](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.position.row == row && IsPlantEconomySeed(state, plant.seedType);
    }));
}

int ZombieLaneAttackScore(const VSGameState &state, int row) {
    if (IsMowerInMotion(state, row) || IsMowerAboutToTrigger(state, row)) {
        // A triggered mower or an intruder already in column zero makes this
        // row a guaranteed whole-lane clear, not an attack opportunity.
        return std::numeric_limits<int>::min() / 4;
    }
    const PlantLaneAssessment assessment = AssessPlantLane(state, row);
    const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
    const int zombieCount = CountZombiesInRow(state, row);
    const int economyPlants = EconomyPlantsInRow(state, row);
    const int graveThreat = GraveThreatScore(state, row);
    int score = PlantLaneWeaknessScore(state, row);

    // Sunflowers and other economy plants are the most efficient pressure
    // targets. Empty rows are still useful for forcing the plant player to
    // spend resources, but are less valuable than a developed economy lane.
    // Separate Sunflower lanes are pressure targets in their own right. A
    // fresh economy lane should outrank feeding a second cheap zombie into a
    // defended lane that one Ash card can erase.
    score += economyPlants * 210 + std::max(0, economyPlants - 1) * 90;
    score += assessment.plantCount == 0 ? 28 : 0;
    score += assessment.defense < 100 ? 35 : 0;
    score += graveThreat * 3;
    score += std::max(0, 32 - firepower.dps) * 3;
    score += firepower.deficit * 5;
    score += !firepower.canHold && firepower.nearHealth > 0 ? 80 : 0;

    const bool mowerGone = row < static_cast<int>(state.mowerAvailable.size())
        && !state.mowerAvailable[static_cast<std::size_t>(row)] && assessment.plantCount > 0;
    if (mowerGone) {
        // A mowerless lane is a conversion route.  Do not abandon the front
        // which paid to remove its mower, but never restart it after that
        // front has died. The next body belongs on a fresh economy lane.
        score += zombieCount > 0 ? 500 : -420;
    }

    // Spread the opening across lanes. A single zombie is useful as a probe;
    // additional zombies in that lane receive a progressively larger penalty.
    const bool pursuingMowerlessLane = mowerGone && !IsMowerInMotion(state, row) && zombieCount > 0;
    if (zombieCount == 0) {
        score += mowerGone ? -240 : 150;
    } else if (zombieCount == 1) {
        score += pursuingMowerlessLane ? 60 : -115;
    } else {
        score -= (pursuingMowerlessLane ? 40 : 95) + (zombieCount - 1) * (pursuingMowerlessLane ? 90 : 175);
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

} // namespace vsai::detail
