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

int PlantThreatScore(const VSGameState &state, int row) {
    int score = CountZombiesInRow(state, row) * 24;
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead || zombie.row != row) {
            continue;
        }
        score += std::max(0, 900 - static_cast<int>(zombie.positionX)) / 5;
        score += zombie.eating ? 130 : 0;
        score += IsHeavyZombie(zombie.zombieType) ? 80 : 0;
        score += IsFastZombie(zombie.zombieType) ? 35 : 0;
        score += zombie.shieldHealth / 30;
    }
    return score;
}

int MostThreatenedRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        const int score = PlantThreatScore(state, row);
        if (score > bestScore) {
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

int MostValuablePlantRow(const VSGameState &state) {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        int score = 0;
        for (const VSPlantState &plant : state.plants) {
            if (!IsDeadOrOutside(plant) && plant.position.row == row) {
                score += PlantValueScore(plant);
            }
        }
        score += CountZombiesInRow(state, row) * 3;
        if (score > bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

VSGridPosition FindPlantCell(const VSGameState &state, int preferredRow, int preferredColumn) {
    preferredRow = std::clamp(preferredRow, 0, std::max(0, state.rows - 1));
    preferredColumn = std::clamp(preferredColumn, 0, 5);
    for (int rowOffset = 0; rowOffset < state.rows; ++rowOffset) {
        const int row = (preferredRow + rowOffset) % state.rows;
        for (int columnOffset = 0; columnOffset <= 5; ++columnOffset) {
            const int column = (preferredColumn + columnOffset) % 6;
            const VSGridPosition position{static_cast<std::int8_t>(column), static_cast<std::int8_t>(row)};
            if (!HasPlantAt(state, position) && !HasGridItemAt(state, position)) {
                return position;
            }
        }
    }
    return {};
}

VSGridPosition FindZombieCell(const VSGameState &state, int row) {
    row = std::clamp(row, 0, std::max(0, state.rows - 1));
    return {static_cast<std::int8_t>(8), static_cast<std::int8_t>(row)};
}

bool IsReadyCard(const VSCardState &card, int resource) {
    return card.seedType != static_cast<std::uint16_t>(SeedType::SEED_NONE) && card.active && !card.refreshing && card.refreshCounter <= 0 && card.cost <= resource;
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
    static bool IsEmergencySeed(std::uint16_t seed) {
        switch (static_cast<SeedType>(seed)) {
            case SeedType::SEED_CHERRYBOMB:
            case SeedType::SEED_SQUASH:
            case SeedType::SEED_JALAPENO:
            case SeedType::SEED_ICESHROOM:
            case SeedType::SEED_DOOMSHROOM:
                return true;
            default:
                return false;
        }
    }

    static bool IsDefenseSeed(std::uint16_t seed) {
        switch (static_cast<SeedType>(seed)) {
            case SeedType::SEED_WALLNUT:
            case SeedType::SEED_TALLNUT:
            case SeedType::SEED_PUMPKINSHELL:
                return true;
            default:
                return false;
        }
    }

    static int CardScore(const VSCardState &card, const VSGameState &state, int threatenedRow) {
        const SeedType seed = static_cast<SeedType>(card.seedType);
        const VSZombieState *closest = FindClosestZombie(state, threatenedRow);
        const bool hasZombie = closest != nullptr;
        const bool closeThreat = hasZombie && (closest->positionX < 520.0f || closest->eating);
        const bool heavyThreat = std::any_of(state.zombies.begin(), state.zombies.end(), [threatenedRow](const VSZombieState &zombie) {
            return !zombie.dead && zombie.row == threatenedRow && IsHeavyZombie(zombie.zombieType);
        });
        const bool fastThreat = std::any_of(state.zombies.begin(), state.zombies.end(), [threatenedRow](const VSZombieState &zombie) {
            return !zombie.dead && zombie.row == threatenedRow && IsFastZombie(zombie.zombieType);
        });

        int score = 20;
        switch (seed) {
            case SeedType::SEED_SQUASH:
                // The replay uses Squash as the immediate answer to armored pushes and sled lanes.
                score += heavyThreat ? 310 : (closeThreat ? 230 : 35);
                break;
            case SeedType::SEED_CHERRYBOMB:
            case SeedType::SEED_JALAPENO:
            case SeedType::SEED_ICESHROOM:
            case SeedType::SEED_DOOMSHROOM:
                score += heavyThreat ? 240 : (closeThreat ? 190 : 25);
                break;
            case SeedType::SEED_BONK_CHOY:
                // Bonk Choy holds a selected lane against Pail, Bobsled, and football pressure.
                score += hasZombie ? 105 : 40;
                score += (heavyThreat || fastThreat) ? 150 : 0;
                break;
            case SeedType::SEED_SNOWPEA:
                // Slow is the durable answer to the replay's Bobsled and Giga Football tempo.
                score += hasZombie ? 95 : 45;
                score += fastThreat ? 150 : 0;
                score += heavyThreat ? 80 : 0;
                break;
            case SeedType::SEED_CELERY_STALKER:
                score += hasZombie ? 130 : 35;
                score += closeThreat ? 70 : 0;
                break;
            case SeedType::SEED_WALLNUT:
            case SeedType::SEED_TALLNUT:
            case SeedType::SEED_PUMPKINSHELL:
                score += closeThreat ? 210 : 35;
                score += CountPlantsInRow(state, threatenedRow) < 3 ? 65 : 0;
                break;
            case SeedType::SEED_SUNFLOWER:
            case SeedType::SEED_SUNSHROOM:
                score += hasZombie ? 5 : 115;
                break;
            case SeedType::SEED_PEASHOOTER:
            case SeedType::SEED_REPEATER:
            case SeedType::SEED_THREEPEATER:
            case SeedType::SEED_GATLINGPEA:
            case SeedType::SEED_MELONPULT:
            case SeedType::SEED_WINTERMELON:
                score += hasZombie ? 100 : 60;
                break;
            default:
                score += IsEmergencySeed(card.seedType) ? (hasZombie ? 150 : 20) : 45;
                break;
        }
        score -= card.cost / 50;
        return score;
    }

public:
    std::optional<VSAction> Decide(const VSGameState &state) override {
        AdvanceBlockedSlots();
        for (const VSResourceState &resource : state.resources) {
            if (resource.side == VSSide::Plants && !resource.dead && !resource.beingCollected) {
                return VSAction{.side = VSSide::Plants, .kind = VSActionKind::CollectResource, .objectId = resource.id, .sequence = ++mSequence};
            }
        }

        const int threatenedRow = MostThreatenedRow(state);
        const VSZombieState *closest = FindClosestZombie(state, threatenedRow);
        const bool underPressure = closest != nullptr && (closest->positionX < 520.0f || closest->eating);
        const VSCardState *bestCard = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const VSCardState &card : state.seedBanks[0]) {
            if (IsSlotBlocked(card.slot) || !IsReadyCard(card, state.plantSun)) {
                continue;
            }
            int score = CardScore(card, state, threatenedRow);
            if (underPressure == IsEmergencySeed(card.seedType)) {
                score += 35;
            }
            if (bestCard == nullptr || score > bestScore) {
                bestCard = &card;
                bestScore = score;
            }
        }
        if (bestCard == nullptr) {
            return std::nullopt;
        }

        const SeedType seed = static_cast<SeedType>(bestCard->seedType);
        int targetColumn = IsDefenseSeed(bestCard->seedType) ? 4 : 2;
        if (IsEmergencySeed(bestCard->seedType)) {
            targetColumn = closest == nullptr ? 4 : std::clamp(static_cast<int>(closest->positionX / 80.0f), 0, 5);
        }
        if (seed == SeedType::SEED_BONK_CHOY || seed == SeedType::SEED_CELERY_STALKER) {
            targetColumn = closest == nullptr ? 3 : std::clamp(static_cast<int>(closest->positionX / 80.0f) - 1, 1, 5);
        }
        if (seed == SeedType::SEED_SNOWPEA) {
            targetColumn = 2;
        }
        if (seed == SeedType::SEED_COBCANNON) {
            targetColumn = 2;
        }
        const VSGridPosition target = FindPlantCell(state, threatenedRow, targetColumn);
        if (target.col < 0 || target.row < 0) {
            return std::nullopt;
        }
        return MakePlayAction(VSSide::Plants, *bestCard, target, state.boardTick);
    }
};

class ZombieVSAgent final : public BuiltinVSAgent {
    static bool IsTargetedSeed(std::uint16_t seed) {
        const SeedType seedType = static_cast<SeedType>(seed);
        return seedType == SeedType::SEED_ZOMBIE_BUNGEE;
    }

    static int CardScore(const VSCardState &card, const VSGameState &state, int targetRow) {
        const SeedType seed = static_cast<SeedType>(card.seedType);
        const bool hasPlants = std::any_of(state.plants.begin(), state.plants.end(), [](const VSPlantState &plant) { return !IsDeadOrOutside(plant); });
        const bool hasSnowPea = HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, targetRow);
        const bool hasBonkChoy = HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, targetRow);
        const bool hasWallnut = HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, targetRow) || HasPlantTypeInRow(state, SeedType::SEED_TALLNUT, targetRow);
        const int plantCount = CountPlantsInRow(state, targetRow);

        int score = 25;
        switch (seed) {
            case SeedType::SEED_ZOMBIE_BOBSLED:
                // Sleds are sent where Snow Pea/Bonk Choy is buying the plants time.
                score += plantCount * 16 + (hasSnowPea ? 190 : 0) + (hasBonkChoy ? 120 : 0);
                break;
            case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
                score += plantCount * 12 + (hasSnowPea ? 115 : 0) + (hasWallnut ? 80 : 0);
                break;
            case SeedType::SEED_ZOMBIE_PAIL:
                // Bucket armor is the cheap answer to lane plants and Snow Pea chip damage.
                score += plantCount * 14 + (hasSnowPea ? 135 : 0) + (hasBonkChoy ? 100 : 0);
                break;
            case SeedType::SEED_ZOMBIE_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR:
            case SeedType::SEED_ZOMBIE_GIGA_FOOTBALL:
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
            case SeedType::SEED_ZOMBIE_MOUND:
            case SeedType::SEED_ZOMBIE_GRAVESTONE:
                score += 95 + plantCount * 4;
                break;
            default:
                score += plantCount * 7;
                break;
        }
        score -= card.cost / 50;
        return score;
    }

public:
    std::optional<VSAction> Decide(const VSGameState &state) override {
        AdvanceBlockedSlots();
        for (const VSResourceState &resource : state.resources) {
            if (resource.side == VSSide::Zombies && !resource.dead && !resource.beingCollected) {
                return VSAction{.side = VSSide::Zombies, .kind = VSActionKind::CollectResource, .objectId = resource.id, .sequence = ++mSequence};
            }
        }

        const VSCardState *bestCard = nullptr;
        int targetRow = MostValuablePlantRow(state);
        int bestScore = std::numeric_limits<int>::min();
        for (const VSCardState &card : state.seedBanks[1]) {
            if (IsSlotBlocked(card.slot) || !IsReadyCard(card, state.zombieBrains)) {
                continue;
            }
            for (int row = 0; row < state.rows; ++row) {
                const int score = CardScore(card, state, row);
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

        VSGridPosition target = FindZombieCell(state, targetRow);
        if (IsTargetedSeed(bestCard->seedType)) {
            const VSPlantState *targetPlant = nullptr;
            for (const VSPlantState &plant : state.plants) {
                if (IsDeadOrOutside(plant)) {
                    continue;
                }
                const int plantScore = PlantValueScore(plant) + static_cast<int>(plant.position.col) * 6
                    + (plant.seedType == static_cast<std::uint16_t>(SeedType::SEED_WALLNUT) ? 45 : 0)
                    + (plant.seedType == static_cast<std::uint16_t>(SeedType::SEED_BONK_CHOY) ? 40 : 0);
                const int currentScore = targetPlant == nullptr ? std::numeric_limits<int>::min()
                                                                 : PlantValueScore(*targetPlant) + static_cast<int>(targetPlant->position.col) * 6
                                                                       + (targetPlant->seedType == static_cast<std::uint16_t>(SeedType::SEED_WALLNUT) ? 45 : 0)
                                                                       + (targetPlant->seedType == static_cast<std::uint16_t>(SeedType::SEED_BONK_CHOY) ? 40 : 0);
                if (targetPlant == nullptr || plantScore > currentScore) {
                    targetPlant = &plant;
                }
            }
            if (targetPlant == nullptr) {
                return std::nullopt;
            }
            target = targetPlant->position;
        }
        return MakePlayAction(VSSide::Zombies, *bestCard, target, state.boardTick);
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
