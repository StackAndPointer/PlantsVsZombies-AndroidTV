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

#include "VSActionAIInternal.h"

#include "PvZ/Lawn/Board/GridItem.h"

#include <algorithm>
#include <limits>

namespace vsai::detail {

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

} // namespace vsai::detail
