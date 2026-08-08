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
        case ZombieType::ZOMBIE_GARGANTUAR:
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

bool HasDecisiveCounterZombieInRow(const VSGameState &state, int row) {
    return std::any_of(state.zombies.begin(), state.zombies.end(), [row](const VSZombieState &zombie) {
        return !zombie.dead && zombie.row == row && IsDecisiveCounterZombie(zombie.zombieType);
    });
}

bool HasZombieTypeInRow(const VSGameState &state, int row, ZombieType zombieType) {
    return std::any_of(state.zombies.begin(), state.zombies.end(), [row, zombieType](const VSZombieState &zombie) {
        return !zombie.dead && zombie.row == row && zombie.zombieType == static_cast<std::uint16_t>(zombieType);
    });
}

int ZombieThreatWeight(std::uint16_t zombieType) {
    switch (static_cast<ZombieType>(zombieType)) {
        case ZombieType::ZOMBIE_GIGA_GARGANTUAR:
        case ZombieType::ZOMBIE_GARGANTUAR:
        case ZombieType::ZOMBIE_GIGA_FOOTBALL:
            return 115;
        case ZombieType::ZOMBIE_BOBSLED:
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
        case SeedType::SEED_BONK_CHOY:
        case SeedType::SEED_CELERY_STALKER:
            score = 65;
            break;
        case SeedType::SEED_IMP_PEAR:
            score = 55;
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
        case SeedType::SEED_BONK_CHOY:
        case SeedType::SEED_CELERY_STALKER:
            score += 75;
            break;
        case SeedType::SEED_STARFRUIT:
            score += 100;
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
        case SeedType::SEED_STARFRUIT:
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_GLOOMSHROOM:
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_WINTERMELON:
        case SeedType::SEED_COBCANNON:
            return true;
        default:
            return false;
    }
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
    if (IsPlantCombatSeed(plant.seedType)) {
        return rowDistance == 0 && plant.position.col < economy.position.col ? 45 : 0;
    }
    return 0;
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

    int score = assessment.plantCount * 14 + economyPlants * 45 + highValuePlants * 24;
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
    score += economyPlants * 58;
    score += assessment.plantCount == 0 ? 28 : 0;
    score += assessment.defense < 100 ? 35 : 0;
    score += graveThreat * 3;

    // Spread the opening across lanes. A single zombie is useful as a probe;
    // additional zombies in that lane receive a progressively larger penalty.
    if (zombieCount == 0) {
        score += 70;
    } else if (zombieCount == 1) {
        score += 10;
    } else {
        score -= (zombieCount - 1) * 72;
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

int CountZombieEconomy(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.gridItems.begin(), state.gridItems.end(), [](const VSGridItemState &item) {
        return !item.dead && (item.gridItemType == GridItemType::GRIDITEM_GRAVESTONE || item.gridItemType == GridItemType::GRIDITEM_MP_BURIAL_MOUND);
    }));
}

bool IsReadyCard(const VSCardState &card, int resource) {
    return card.seedType != static_cast<std::uint16_t>(SeedType::SEED_NONE) && card.active && !card.refreshing && card.refreshCounter <= 0 && card.cost <= resource;
}

bool IsAreaCounterSeed(SeedType seed) {
    return seed == SeedType::SEED_SQUASH || seed == SeedType::SEED_CHERRYBOMB || seed == SeedType::SEED_JALAPENO
        || seed == SeedType::SEED_ICESHROOM || seed == SeedType::SEED_DOOMSHROOM;
}

bool IsHeavyZombieSeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_GARGANTUAR || seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR
        || seed == SeedType::SEED_ZOMBIE_GIGA_FOOTBALL;
}

bool HasZombieCluster(const VSGameState &state) {
    if (CountActiveZombies(state) >= 3) {
        return true;
    }
    for (int row = 0; row < state.rows; ++row) {
        if (CountZombiesInRow(state, row) >= 2) {
            return true;
        }
    }
    return false;
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

    std::optional<VSAction> TryIncomePlant(const VSGameState &state, int row) {
        if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_SUNFLOWER, row, 0, 2)) {
            return action;
        }
        return TryPlant(state, SeedType::SEED_SUNSHROOM, row, 0, 2);
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
            if (seed == SeedType::SEED_SUNFLOWER || seed == SeedType::SEED_SUNSHROOM || seed == SeedType::SEED_IMP_PEAR) {
                continue;
            }
            const bool emergencySeed = seed == SeedType::SEED_SQUASH || seed == SeedType::SEED_CHERRYBOMB || seed == SeedType::SEED_JALAPENO
                || seed == SeedType::SEED_ICESHROOM || seed == SeedType::SEED_DOOMSHROOM;
            if (emergencySeed && (!hasActiveZombie || danger.danger < 150)) {
                continue;
            }

            if (seed == SeedType::SEED_SQUASH
                && (danger.closest == nullptr || (!HasDecisiveCounterZombieInRow(state, danger.row) && !danger.closest->eating && danger.closest->positionX >= 560.0f))) {
                continue;
            }

            if (seed == SeedType::SEED_SNOWPEA && HasPlantTypeInRow(state, seed, danger.danger >= 105 ? danger.row : buildRow)) {
                continue;
            }

            if ((seed == SeedType::SEED_WALLNUT || seed == SeedType::SEED_TALLNUT || seed == SeedType::SEED_PUMPKINSHELL)
                && (!hasActiveZombie || danger.closest == nullptr || danger.danger < 90)) {
                continue;
            }

            int row = danger.danger >= 105 ? danger.row : buildRow;
            int firstColumn = 2;
            int lastColumn = 3;
            if (emergencySeed) {
                firstColumn = 4;
                lastColumn = 5;
            } else if (seed == SeedType::SEED_WALLNUT || seed == SeedType::SEED_TALLNUT || seed == SeedType::SEED_PUMPKINSHELL) {
                firstColumn = lastColumn = 4;
            } else if (seed == SeedType::SEED_BONK_CHOY || seed == SeedType::SEED_CELERY_STALKER) {
                firstColumn = lastColumn = 3;
            }

            const VSGridPosition target = emergencySeed ? FindPlantCellInExactRow(state, row, firstColumn, lastColumn)
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
        const int incomePlantCount = CountPlantType(state, SeedType::SEED_SUNFLOWER) + CountPlantType(state, SeedType::SEED_SUNSHROOM);
        const bool hasActiveZombie = CountActiveZombies(state) > 0;
        const bool closePush = danger.closest != nullptr && (danger.closest->positionX < 560.0f || danger.closest->eating);
        const bool hasGargantuar = HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_GARGANTUAR)
            || HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_GIGA_GARGANTUAR);
        const bool hasSquashPriorityZombie = HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_BOBSLED)
            || HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_FOOTBALL)
            || HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_GIGA_FOOTBALL)
            || HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_POLEVAULTER);
        const bool squashThreat = hasSquashPriorityZombie || (hasGargantuar && closePush)
            || (danger.closest != nullptr && (danger.closest->eating || danger.closest->positionX < 560.0f));
        const bool impPearThreat = hasGargantuar && danger.closest != nullptr
            && (danger.closest->eating || danger.closest->positionX < 780.0f || danger.danger >= 160);
        const bool emergency = danger.danger >= 240 || (closePush && (danger.hasHeavy || danger.hasFast));
        const bool zombieCluster = hasActiveZombie && HasZombieCluster(state);
        const int areaCounterReserve = AreaCounterReserve(state);

        // The recorded plant side builds its sun base first, then answers a real
        // heavy/fast push with Squash. It is never an opening filler card.
        // Against Gargantuars the replay preserves Imp Pear for the first
        // answer; Squash is the follow-up when the giant reaches the line.
        if (hasActiveZombie && impPearThreat && !HasPlantTypeInRow(state, SeedType::SEED_IMP_PEAR, danger.row)) {
            if (std::optional<VSAction> action = TryPlantExactRow(state, SeedType::SEED_IMP_PEAR, danger.row, 4, 5)) {
                return action;
            }
        }
        if (zombieCluster && danger.closest != nullptr && !HasPlantTypeInRow(state, SeedType::SEED_CHERRYBOMB, danger.row)) {
            if (std::optional<VSAction> action = TryPlantExactRow(state, SeedType::SEED_CHERRYBOMB, danger.row, 3, 5)) {
                return action;
            }
        }
        if (zombieCluster && danger.closest != nullptr && (!hasGargantuar || squashThreat)
            && !HasPlantTypeInRow(state, SeedType::SEED_SQUASH, danger.row)) {
            if (std::optional<VSAction> action = TryPlantExactRow(state, SeedType::SEED_SQUASH, danger.row, 4, 5)) {
                return action;
            }
        } else if (hasActiveZombie && squashThreat && !HasPlantTypeInRow(state, SeedType::SEED_SQUASH, danger.row)) {
            if (std::optional<VSAction> action = TryPlantExactRow(state, SeedType::SEED_SQUASH, danger.row, 4, 5)) {
                return action;
            }
        }
        if (hasActiveZombie && emergency && impPearThreat && !HasPlantTypeInRow(state, SeedType::SEED_IMP_PEAR, danger.row)) {
            if (std::optional<VSAction> action = TryPlantExactRow(state, SeedType::SEED_IMP_PEAR, danger.row, 4, 5)) {
                return action;
            }
        }

        if (zombieCluster && areaCounterReserve > 0 && state.plantSun < areaCounterReserve) {
            // Keep the remaining sun for the first available area answer. A
            // cheap shooter or nut cannot solve a multi-zombie pileup as well
            // as the reserved counter card.
            return std::nullopt;
        }

        if (incomePlantCount < openingIncomeTarget && danger.danger < 150) {
            if (std::optional<VSAction> action = TryIncomePlant(state, LeastDevelopedPlantRow(state))) {
                return action;
            }
            // Safe but unaffordable: wait for sun instead of spending a
            // defensive card merely because it is available.
            if (!hasActiveZombie || danger.danger < 90) {
                return std::nullopt;
            }
        }

        if (!hasActiveZombie) {
            const int buildRow = LeastDevelopedPlantRow(state);
            if (incomePlantCount < openingIncomeTarget + 2) {
                if (std::optional<VSAction> action = TryIncomePlant(state, buildRow)) {
                    return action;
                }
            }
            // Once the replay-like economy is established, pre-build only a
            // combat plant. Nuts and instant counters wait for a visible lane.
            if (!HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, buildRow)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_SNOWPEA, buildRow, 1, 2)) {
                    return action;
                }
            }
            if (!HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, buildRow)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_BONK_CHOY, buildRow, 3, 3)) {
                    return action;
                }
            }
            return std::nullopt;
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
            if (hasActiveZombie && danger.closest != nullptr && !HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_WALLNUT, danger.row, 4, 4)) {
                    return action;
                }
            }
            if (hasActiveZombie && impPearThreat && !HasPlantTypeInRow(state, SeedType::SEED_IMP_PEAR, danger.row)) {
                if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_IMP_PEAR, danger.row, 4, 5)) {
                    return action;
                }
            }
        }

        const int buildRow = LeastDevelopedPlantRow(state);
        if (incomePlantCount < openingIncomeTarget + 2 && danger.danger < 80) {
            if (std::optional<VSAction> action = TryIncomePlant(state, buildRow)) {
                return action;
            }
        }
        if (!HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, buildRow)) {
            if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_SNOWPEA, buildRow, 1, 2)) {
                return action;
            }
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
        if (incomePlantCount < openingIncomeTarget + 4) {
            return TryIncomePlant(state, buildRow);
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
            if (!IsSlotBlocked(card.slot) && card.seedType == static_cast<std::uint16_t>(seedType) && IsReadyCard(card, state.zombieBrains)) {
                return &card;
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
        const bool anyCombatPlant = std::any_of(state.plants.begin(), state.plants.end(), [](const VSPlantState &candidate) {
            return !IsDeadOrOutside(candidate) && IsPlantCombatSeed(candidate.seedType);
        });
        const bool combatPlant = IsPlantCombatSeed(plant.seedType);
        int score = PlantValueScore(plant) + static_cast<int>(plant.position.col) * 8;
        if (combatPlant) {
            score += anyCombatPlant ? 220 : 80;
        } else if (IsPlantEconomySeed(plant.seedType)) {
            // Economy is a fallback target only when the opponent has no
            // combat plant worth stealing.
            score += anyCombatPlant ? -180 : 45;
        }
        score += PlantLaneWeaknessScore(state, row) / 3;
        return score;
    }

    static int CardScore(const VSCardState &card, const VSGameState &state, int targetRow, int economyCount) {
        const SeedType seed = static_cast<SeedType>(card.seedType);
        const bool hasPlants = std::any_of(state.plants.begin(), state.plants.end(), [](const VSPlantState &plant) { return !IsDeadOrOutside(plant); });
        const bool hasSnowPea = HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, targetRow);
        const bool hasBonkChoy = HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, targetRow);
        const bool hasWallnut = HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, targetRow) || HasPlantTypeInRow(state, SeedType::SEED_TALLNUT, targetRow);
        const int plantCount = CountPlantsInRow(state, targetRow);
        const int zombieCount = CountZombiesInRow(state, targetRow);

        int score = 20 + ZombieLaneAttackScore(state, targetRow);
        const int graveThreat = GraveThreatScore(state, targetRow);
        // A grave is the zombie player's income source. Any available
        // pressure is deliberately biased toward a lane that is shooting it.
        score += graveThreat * 2;
        switch (seed) {
            case SeedType::SEED_ZOMBIE_BOBSLED:
                // After the opening graves, the replay's first proactive pressure is Bobsled into a held lane.
                score += 95 + plantCount * 16 + (hasSnowPea ? 190 : 0) + (hasBonkChoy ? 120 : 0);
                break;
            case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
                score += 80 + plantCount * 12 + (hasSnowPea ? 115 : 0) + (hasWallnut ? 80 : 0);
                break;
            case SeedType::SEED_ZOMBIE_PAIL:
                score += 65 + plantCount * 14 + (hasSnowPea ? 135 : 0) + (hasBonkChoy ? 100 : 0);
                break;
            case SeedType::SEED_ZOMBIE_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_FOOTBALL:
                // Once the grave economy is online, this is the decisive
                // release card, but do not spend it into an empty lane.
                score += economyCount >= 4 ? ((zombieCount >= 2 || plantCount >= 3 || graveThreat >= 120) ? 270 : 65) : -140;
                score += plantCount * 18 + (hasWallnut ? 135 : 0) + (hasBonkChoy ? 100 : 0) + (hasSnowPea ? 75 : 0);
                break;
            case SeedType::SEED_ZOMBIE_PEA_HEAD:
            case SeedType::SEED_ZOMBIE_NEWSPAPER:
            case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
                score += plantCount * 10 + (hasSnowPea ? 120 : 0);
                break;
            case SeedType::SEED_ZOMBIE_IMP:
            case SeedType::SEED_ZOMBIE_DIGGER:
                score += plantCount * 8 + (hasWallnut ? 90 : 0);
                break;
            case SeedType::SEED_ZOMBIE_BUNGEE:
                score += hasPlants ? 220 : -80;
                score += (hasWallnut || hasBonkChoy) ? 85 : 0;
                break;
            case SeedType::SEED_ZOMBIE_GRAVESTONE:
                score += economyCount < 4 ? 900 : (economyCount < 8 ? 125 : 15);
                score += plantCount * 4;
                break;
            case SeedType::SEED_ZOMBIE_MOUND:
                score += economyCount > 0 ? 55 : -150;
                score += graveThreat / 2;
                break;
            case SeedType::SEED_ZOMBIE_DANCER:
                score += 75 + plantCount * 12 + (graveThreat > 0 ? 35 : 0);
                break;
            case SeedType::SEED_ZOMBIE_CATAPULT:
            case SeedType::SEED_ZOMBIE_BALLOON:
                score += 65 + plantCount * 8 + (hasSnowPea ? 75 : 0);
                break;
            default:
                score += plantCount * 7;
                break;
        }
        score -= card.cost / 50;
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
        const int graveDefenseRow = MostThreatenedEconomyRow(state);
        const int graveDefenseScore = GraveThreatScore(state, graveDefenseRow);
        const int economicRow = LeastThreatenedEconomyRow(state);
        if (economyCount < 4 && graveDefenseScore < 100) {
            if (std::optional<VSAction> action = TryBuildEconomy(state, economicRow)) {
                return action;
            }
        }

        const int heavyZombieReserve = HeavyZombieReserve(state);
        const bool saveForHeavy = heavyZombieReserve > 0 && economyCount >= 4 && state.zombieBrains < heavyZombieReserve && graveDefenseScore < 100;

        auto FindTarget = [&](const VSCardState &card, int row) -> std::optional<VSGridPosition> {
            const SeedType seed = static_cast<SeedType>(card.seedType);
            if (seed == SeedType::SEED_ZOMBIE_GRAVESTONE) {
                const VSGridPosition target = FindZombieEconomyCell(state, row);
                return target.col >= 0 && target.row >= 0 ? std::optional<VSGridPosition>(target) : std::nullopt;
            }
            if (IsTargetedSeed(card.seedType)) {
                const VSPlantState *targetPlant = nullptr;
                int targetScore = std::numeric_limits<int>::min();
                for (const VSPlantState &plant : state.plants) {
                    if (IsDeadOrOutside(plant) || plant.position.row != row) {
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
            if (IsSlotBlocked(card.slot) || !IsReadyCard(card, state.zombieBrains)) {
                continue;
            }
            if (graveDefenseScore >= 100 && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_ZOMBIE_GRAVESTONE)) {
                continue;
            }
            for (int row = 0; row < state.rows; ++row) {
                if (!FindTarget(card, row).has_value()) {
                    continue;
                }
                int score = CardScore(card, state, row, economyCount);
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
                    score -= 95;
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
        mLastAttackRow = targetRow;
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
