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

#include <algorithm>
#include <limits>
#include <optional>

namespace vsai::detail {

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

    bool HasEconomyPressurePlan(const VSGameState &state) const {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [](const VSCardState &card) {
            if (card.matchRestricted || !card.active) {
                return false;
            }
            const SeedType seed = static_cast<SeedType>(card.seedType);
            return seed == SeedType::SEED_GRAVEBUSTER || IsSustainedOutputSeed(seed);
        });
    }

    int EconomyPressureIncomeTarget(const VSGameState &state) const {
        bool hasGraveBuster = false;
        bool hasCrossLaneOutput = false;
        int cheapestOutputCost = std::numeric_limits<int>::max();
        for (const VSCardState &card : state.seedBanks[0]) {
            if (card.matchRestricted || !card.active) {
                continue;
            }
            const SeedType seed = static_cast<SeedType>(card.seedType);
            if (seed == SeedType::SEED_GRAVEBUSTER) {
                hasGraveBuster = true;
                continue;
            }
            if (!IsSustainedOutputSeed(seed)) {
                continue;
            }
            cheapestOutputCost = std::min(cheapestOutputCost, std::max(0, card.cost));
            hasCrossLaneOutput = hasCrossLaneOutput || seed == SeedType::SEED_STARFRUIT || seed == SeedType::SEED_THREEPEATER;
        }

        // A Gravebuster or cross-lane shooter begins denying tombstone income
        // earlier. Expensive, single-lane damage needs one more producer to
        // maintain both pressure and a defensive reserve. Decks with neither
        // option retain a larger economy because they cannot tax graves yet.
        int target = state.rows + 3;
        target -= hasGraveBuster ? 1 : 0;
        target -= hasCrossLaneOutput ? 1 : 0;
        target += cheapestOutputCost >= 125 ? 1 : 0;
        target += cheapestOutputCost == std::numeric_limits<int>::max() ? 1 : 0;
        return std::clamp(target, std::max(3, state.rows - 1), state.rows * 2);
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
        const bool hasEconomyPressurePlan = HasEconomyPressurePlan(state);
        const int incomeExpansionTarget = state.isSuddenDeath ? 0 : EconomyPressureIncomeTarget(state);
        const bool immediateCounterThreat = squashThreat || impPearThreat;
        const bool mustHoldCounterReserve = areaCounterReserve > 0 && state.plantSun >= areaCounterReserve
            && HasReadyZombieBreakthroughCard(state);
        const int protectedSun = mustHoldCounterReserve ? areaCounterReserve : 0;
        const int zombieEconomyStrikeRow = MostVulnerableZombieEconomyRow(state);
        const bool canStrikeZombieEconomy = (state.isSuddenDeath || incomePlantCount >= minimumIncomeBeforeOutput) && hasEconomyPressurePlan
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
            if (hasIncomeSeed && incomePlantCount < incomeExpansionTarget) {
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

std::unique_ptr<IVSAgent> CreatePlantVSAgent() {
    return std::make_unique<PlantVSAgent>();
}

} // namespace vsai::detail
