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

    static int GraveGuardPriority(SeedType seed) {
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

    std::optional<VSAction> TryProtectEconomy(const VSGameState &state, int row) {
        if (row < 0 || row >= state.rows || HasZombieGraveGuardInRow(state, row) || GraveThreatScore(state, row) < 100) {
            return std::nullopt;
        }

        const VSCardState *bestCard = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const VSCardState &card : state.seedBanks[1]) {
            const SeedType seed = static_cast<SeedType>(card.seedType);
            if (IsSlotBlocked(card.slot) || !IsZombieGraveGuardSeed(seed) || !IsReadyCard(card, state.zombieBrains)) {
                continue;
            }
            const VSGridPosition target = FindZombieCell(state, seed, row);
            if (!IsCardReadyForZombieTarget(card, state, target)) {
                continue;
            }

            int score = GraveGuardPriority(seed) + GraveThreatScore(state, row) * 2;
            score += StraightProjectileThreatScore(state, row) > 0 ? 240 : 0;
            score += StrategyBonus(state, VSSide::Zombies, seed, row);
            score -= card.cost / 4;
            if (bestCard == nullptr || score > bestScore) {
                bestCard = &card;
                bestScore = score;
            }
        }

        if (bestCard == nullptr) {
            return std::nullopt;
        }
        return MakePlayAction(VSSide::Zombies, *bestCard, FindZombieCell(state, static_cast<SeedType>(bestCard->seedType), row), state.boardTick);
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
            case SeedType::SEED_ZOMBIE_TALLNUT_HEAD:
                // Mound games protect the upgraded economic asset with a
                // durable head rather than treating Trashcan as the only
                // viable grave screen.
                score += graveProjectileThreat > 0 && !hasGraveGuard ? 440 + graveProjectileThreat * 2 : -95;
                score += graveThreat >= 100 ? 115 : 0;
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
                    // The new replay starts its first giant once ten to
                    // eleven graves are producing, rather than waiting for a
                    // mathematically perfect rear field.  Require a real
                    // target for this early release so it remains a commit,
                    // not an opening all-in.
                    const bool hasMidGameHeavyEconomy = economyCount >= std::max(state.rows * 2, heavyEconomyThreshold - 2)
                        && hasBreakthroughTarget;
                    score += (economyCount >= heavyEconomyThreshold || hasMidGameHeavyEconomy) ? (hasBreakthroughTarget ? 285 : 35) : -220;
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
            case SeedType::SEED_ZOMBIE_SUPER_FAN_IMP:
                // Cheap fast pressure should fan out through under-defended
                // sunflower lanes, not shadow an existing zombie stack.
                score += 105 + plantCount * 9 + sustainedOutput / 3 + economyValue / 2;
                score += zombieCount == 0 ? 110 : -90;
                break;
            case SeedType::SEED_ZOMBIE_SQUASH_HEAD:
                score += 95 + plantCount * 11 + sustainedOutput / 3 + economyValue / 3;
                score += zombieCount == 0 ? 90 : -75;
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
                // A mound is an upgrade to a grave economy, not a substitute
                // for the initial rear field.
                score += economyCount >= std::max(3, state.rows / 2) ? 220 : -220;
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
        const int economyTarget = state.isSuddenDeath ? economyCount : state.rows * 3;
        const int economyDeficit = std::max(0, economyTarget - economyCount);
        const int graveDefenseRow = MostThreatenedEconomyRow(state);
        const int graveDefenseScore = GraveThreatScore(state, graveDefenseRow);
        const bool hasGraveGuard = HasZombieGraveGuardInRow(state, graveDefenseRow);
        const bool graveDefenseUrgent = graveDefenseScore >= 100;
        const bool graveDefenseUncovered = graveDefenseUrgent && !hasGraveGuard;
        if (graveDefenseUncovered) {
            if (std::optional<VSAction> action = TryProtectEconomy(state, graveDefenseRow)) {
                return action;
            }
        }
        const int activePressureRows = CountActiveZombieRows(state);
        const int survivingFrontRow = MostValuableZombieFrontRow(state);
        const int survivingFrontValue = ZombieFrontlineValueInRow(state, survivingFrontRow);
        const bool preserveSurvivingFront = economyCount >= state.rows && activePressureRows == 1 && survivingFrontValue >= 90;
        const bool survivingFrontGuarded = HasZombieGraveGuardInRow(state, survivingFrontRow);
        const int economicRow = economyCount < state.rows * 2 ? LeastCommittedZombieRow(state) : LeastThreatenedEconomyRow(state);
        const bool restorationCanProceed = !graveDefenseUncovered || hasGraveGuard;
        const bool restorationOutweighsFront = economyDeficit >= 2 || graveDefenseScore < 100 || hasGraveGuard;
        if (economyDeficit > 0 && restorationCanProceed && (!preserveSurvivingFront || restorationOutweighsFront)) {
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
            if (seed == SeedType::SEED_ZOMBIE_TRASHCAN || seed == SeedType::SEED_ZOMBIE_TALLNUT_HEAD) {
                // Trashcan advances too slowly to be an attacking probe. Its
                // job is to absorb direct fire before it reaches a grave;
                // Tall-nut Head takes the same role in mound decks.
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
        int targetRow = graveDefenseUrgent ? graveDefenseRow : MostVulnerablePlantRow(state);
        int bestScore = std::numeric_limits<int>::min();
        for (const VSCardState &card : state.seedBanks[1]) {
            if (IsSlotBlocked(card.slot) || !card.active || card.refreshing || card.refreshCounter > 0) {
                continue;
            }
            const SeedType seed = static_cast<SeedType>(card.seedType);
            // Sudden death removes zombie-side economy actions.  Filter them
            // before scoring so an otherwise attractive grave cannot stall
            // the agent on a target the mode rejects.
            if (state.isSuddenDeath && (seed == SeedType::SEED_ZOMBIE_GRAVESTONE || seed == SeedType::SEED_ZOMBIE_MOUND)) {
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
                const bool isEconomyAction = seed == SeedType::SEED_ZOMBIE_GRAVESTONE || seed == SeedType::SEED_ZOMBIE_MOUND;
                const bool isTargetedAction = IsTargetedSeed(card.seedType);
                const bool isProtectedGuard = IsZombieGraveGuardSeed(seed) && graveDefenseUrgent;
                const int zombiesInRow = CountZombiesInRow(state, row);
                const int desiredOpeningRows = std::min(3, state.rows);
                if (!isEconomyAction && !isTargetedAction && !isProtectedGuard && !IsHeavyZombieSeed(seed)
                    && activePressureRows < desiredOpeningRows) {
                    // The new recordings use cheap cones, imps and normal
                    // zombies to establish several live probes before any
                    // lane receives a second body. This also denies one Ash
                    // counter an entire zombie-side wave.
                    score += zombiesInRow == 0 ? 210 : -280;
                }
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

std::unique_ptr<IVSAgent> CreateZombieVSAgent() {
    return std::make_unique<ZombieVSAgent>();
}

} // namespace vsai::detail
