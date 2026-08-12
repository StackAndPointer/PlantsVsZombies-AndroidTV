#include "PlantAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include "PvZ/Lawn/Common/GameConstants.h"

namespace vsai::detail {
std::optional<VSAction> PlantAIPlanning::TryPeaDoomTempoPressure(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };

    // This recording's Peashooter is an early economic threat. Doom and
    // Chilly are held to answer a later swarm; they must not make the AI
    // wait for four flowers before its first low-cost firing lane exists.
    const bool peaDoomTemplate = HasActiveSeed(SeedType::SEED_PEASHOOTER)
        && HasActiveSeed(SeedType::SEED_DOOMSHROOM) && HasActiveSeed(SeedType::SEED_CHILLY_PEPPER)
        && HasActiveSeed(SeedType::SEED_HYPNOSHROOM);
    if (!peaDoomTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 1 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }

    const VSCardState *pea = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_PEASHOOTER);
    if (pea == nullptr || state.plantSun - pea->cost < protectedSun) {
        return std::nullopt;
    }
    const int peaTarget = EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 4 ? 1 : std::min(state.rows, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 2);
    if (CountPlantType(state, SeedType::SEED_PEASHOOTER) >= peaTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_PEASHOOTER, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)
            || IsRangedOutputTradeUnfavorable(state, row)) {
            continue;
        }
        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_PEASHOOTER, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_PEASHOOTER, target)) {
            continue;
        }
        const VSZombieState *closest = FindClosestZombie(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_PEASHOOTER, row) * 8;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 13;
        score += closest == nullptr ? 70 : (closest->positionX > 640.0f ? 80 : -120);
        score += row == preferredRow ? 35 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_PEASHOOTER, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 ? std::nullopt
                              : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *pea, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TryStarfruitCrossfireFormation(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };

    // Starfruit recordings build from the inner rows so each plant applies
    // crossfire to three lanes. Puff/Chomper are local answers and must not
    // postpone that central pressure formation.
    const bool starfruitTemplate = HasActiveSeed(SeedType::SEED_STARFRUIT)
        && (HasActiveSeed(SeedType::SEED_PUFFSHROOM) || HasActiveSeed(SeedType::SEED_CHOMPER));
    if (!starfruitTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 4 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }
    const VSCardState *starfruit = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_STARFRUIT);
    if (starfruit == nullptr || state.plantSun - starfruit->cost < protectedSun) {
        return std::nullopt;
    }
    const int crossfireTarget = std::min(state.rows, std::max(1, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 3));
    if (CountPlantType(state, SeedType::SEED_STARFRUIT) >= crossfireTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (row == 0 || row == state.rows - 1 || HasPlantTypeInRow(state, SeedType::SEED_STARFRUIT, row)
            || PlantAIPlanning::ShouldYieldLaneToMower(state, row) || IsRangedOutputTradeUnfavorable(state, row)) {
            continue;
        }
        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_STARFRUIT, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_STARFRUIT, target)) {
            continue;
        }
        int crossfireValue = 0;
        int crossfireDeficit = 0;
        for (int coveredRow = row - 1; coveredRow <= row + 1; ++coveredRow) {
            crossfireValue += PlantEconomyValueInRow(state, coveredRow);
            crossfireDeficit += AssessPlantLaneFirepower(state, coveredRow).deficit;
        }
        const VSZombieState *closest = FindClosestZombie(state, row);
        int score = crossfireValue * 3 + crossfireDeficit * 9;
        score += SeedEconomyPressureOpportunity(state, SeedType::SEED_STARFRUIT, row) * 6;
        score += closest == nullptr ? 60 : (closest->positionX > 640.0f ? 70 : -115);
        score += row == preferredRow ? 30 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_STARFRUIT, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 ? std::nullopt
                              : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *starfruit, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TryCactusSpikeweedCore(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };
    const bool cactusSpikeweedTemplate = HasActiveSeed(SeedType::SEED_CACTUS) && HasActiveSeed(SeedType::SEED_SPIKEWEED)
        && HasActiveSeed(SeedType::SEED_POTATOMINE);
    if (!cactusSpikeweedTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 4 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }
    const VSCardState *cactus = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_CACTUS);
    if (cactus == nullptr || state.plantSun - cactus->cost < protectedSun) {
        return std::nullopt;
    }
    const int cactusTarget = std::min(state.rows, std::max(1, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 3));
    if (CountPlantType(state, SeedType::SEED_CACTUS) >= cactusTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_CACTUS, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)
            || IsRangedOutputTradeUnfavorable(state, row)) {
            continue;
        }
        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_CACTUS, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_CACTUS, target)) {
            continue;
        }
        const VSZombieState *closest = FindClosestZombie(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_CACTUS, row) * 7;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 13;
        score += closest == nullptr ? 60 : (closest->positionX > 640.0f ? 75 : -125);
        score += row == preferredRow ? 30 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_CACTUS, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 ? std::nullopt
                              : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *cactus, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TryKernelCeleryFormation(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };
    // Kernel-pult is the durable lane pressure in the Kernel/Celery replay.
    // Celery reacts at the front only after this lobbed firing core exists.
    const bool kernelCeleryTemplate = HasActiveSeed(SeedType::SEED_KERNELPULT) && HasActiveSeed(SeedType::SEED_CELERY_STALKER)
        && HasActiveSeed(SeedType::SEED_POTATOMINE);
    if (!kernelCeleryTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 6 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }
    const VSCardState *kernel = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_KERNELPULT);
    if (kernel == nullptr || state.plantSun - kernel->cost < protectedSun) {
        return std::nullopt;
    }
    const int kernelTarget = std::min(state.rows, std::max(1, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 5));
    if (CountPlantType(state, SeedType::SEED_KERNELPULT) >= kernelTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_KERNELPULT, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)) {
            continue;
        }
        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_KERNELPULT, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_KERNELPULT, target)) {
            continue;
        }
        const VSZombieState *closest = FindClosestZombie(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_KERNELPULT, row) * 8;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 12;
        score += closest == nullptr ? 55 : (closest->positionX > 620.0f ? 70 : -85);
        score += row == preferredRow ? 30 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_KERNELPULT, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 ? std::nullopt
                              : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *kernel, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TryRepeaterCeleryTempo(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };

    // This replay spends its first three producers on a two-lane Repeater
    // core. Celery is a close-range response only; letting it consume the
    // early firing budget turns the deck into a fragile melee opening.
    const bool repeaterCeleryTemplate = HasActiveSeed(SeedType::SEED_REPEATER)
        && HasActiveSeed(SeedType::SEED_CELERY_STALKER) && HasActiveSeed(SeedType::SEED_JALAPENO)
        && (HasActiveSeed(SeedType::SEED_WALLNUT) || HasActiveSeed(SeedType::SEED_TALLNUT));
    if (!repeaterCeleryTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 3 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }

    const VSCardState *repeater = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_REPEATER);
    const int totalCost = repeater == nullptr ? std::numeric_limits<int>::max() : PlantAIPlanning::EffectivePlantPlayCost(state, *repeater);
    if (repeater == nullptr || totalCost == std::numeric_limits<int>::max() || state.plantSun - totalCost < protectedSun) {
        return std::nullopt;
    }
    const int firingLineTarget = std::min(state.rows, std::max(2, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 1));
    if (CountPlantType(state, SeedType::SEED_REPEATER) >= firingLineTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_REPEATER, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)
            || IsRangedOutputTradeUnfavorable(state, row)) {
            continue;
        }
        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_REPEATER, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_REPEATER, target)) {
            continue;
        }
        const VSZombieState *closest = FindClosestZombie(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_REPEATER, row) * 8;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 13;
        score += closest == nullptr ? 70 : (closest->positionX > 640.0f ? 80 : -135);
        score += row == preferredRow ? 35 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_REPEATER, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 ? std::nullopt
                              : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *repeater, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TryMelonMineTempo(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };

    // In the pure Melon-pult recording, Potato Mine and Wall-nut buy the
    // first firing window. There is no Scaredy-shroom support layer: once
    // the saved sun reaches Melon cost, pressure the grave economy directly.
    const bool melonMineTemplate = HasActiveSeed(SeedType::SEED_MELONPULT)
        && HasActiveSeed(SeedType::SEED_POTATOMINE) && !HasActiveSeed(SeedType::SEED_SCAREDYSHROOM)
        && (HasActiveSeed(SeedType::SEED_WALLNUT) || HasActiveSeed(SeedType::SEED_CHILLY_PEPPER));
    if (!melonMineTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 3 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }

    const VSCardState *melon = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_MELONPULT);
    const int totalCost = melon == nullptr ? std::numeric_limits<int>::max() : PlantAIPlanning::EffectivePlantPlayCost(state, *melon);
    if (melon == nullptr || totalCost == std::numeric_limits<int>::max() || state.plantSun - totalCost < protectedSun) {
        return std::nullopt;
    }
    const int firingLineTarget = std::min(state.rows, std::max(1, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 2));
    if (CountPlantType(state, SeedType::SEED_MELONPULT) >= firingLineTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_MELONPULT, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)) {
            continue;
        }
        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_MELONPULT, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_MELONPULT, target)) {
            continue;
        }
        const VSZombieState *closest = FindClosestZombie(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_MELONPULT, row) * 9;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 12;
        score += closest == nullptr ? 60 : (closest->positionX > 620.0f ? 70 : -80);
        score += row == preferredRow ? 35 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_MELONPULT, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 ? std::nullopt
                              : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *melon, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TryRepeaterTempoPressure(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };

    // The Repeater/Sun-shroom recordings use two actual Sunflowers plus a
    // disposable Sun-shroom pad, then immediately turn the first affordable
    // 200 sun into a rear firing lane. Daytime Sun-shrooms do not count as
    // income, but they do make the two-producer breakpoint safe enough to
    // start grave pressure before the generic filler can spend it.
    const bool repeaterTempoTemplate = HasActiveSeed(SeedType::SEED_REPEATER)
        && HasActiveSeed(SeedType::SEED_SUNSHROOM) && HasActiveSeed(SeedType::SEED_WALLNUT)
        && (HasActiveSeed(SeedType::SEED_SQUASH) || HasActiveSeed(SeedType::SEED_CHILLY_PEPPER));
    const int incomeCount = EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state));
    const bool hasSunshroomPad = CountPlantType(state, SeedType::SEED_SUNSHROOM) > 0;
    if (!repeaterTempoTemplate || incomeCount < (hasSunshroomPad ? 2 : 3) || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }

    const VSCardState *repeater = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_REPEATER);
    const int totalCost = repeater == nullptr ? std::numeric_limits<int>::max() : PlantAIPlanning::EffectivePlantPlayCost(state, *repeater);
    if (repeater == nullptr || totalCost == std::numeric_limits<int>::max() || state.plantSun - totalCost < protectedSun) {
        return std::nullopt;
    }

    // Spread the opening repeaters across safe grave lanes. Later generic
    // output logic may deepen a completed route, but the first pass must not
    // hand a Cherry or Squash a stack of the only durable plant-side carry.
    const int openingTarget = std::min(state.rows, std::max(1, (incomeCount + 1) / 2));
    if (CountPlantType(state, SeedType::SEED_REPEATER) >= openingTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_REPEATER, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)
            || IsRangedOutputTradeUnfavorable(state, row)) {
            continue;
        }

        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_REPEATER, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_REPEATER, target)) {
            continue;
        }

        const PlantLaneAssessment lane = AssessPlantLane(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        const VSZombieState *closest = FindClosestZombie(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_REPEATER, row) * 8;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 14;
        score += closest == nullptr ? 55 : (closest->positionX > 620.0f ? 80 : -115);
        score += lane.danger < 105 ? 65 : -120;
        score += row == preferredRow ? 35 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_REPEATER, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 || bestTarget.row < 0
        ? std::nullopt
        : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *repeater, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TrySporeShellPressure(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };

    // The Pumpkin/Squash Spore recordings establish a small rear firing
    // spread before spending either defensive card. Pumpkin protects a
    // developed carry and Squash clears a real breakthrough; neither is an
    // opening substitute for the lobbed pressure that can damage graves
    // through a slow zombie screen.
    const bool sporeShellTemplate = HasActiveSeed(SeedType::SEED_SPORESHROOM)
        && HasActiveSeed(SeedType::SEED_PUMPKINSHELL)
        && (HasActiveSeed(SeedType::SEED_SQUASH) || HasActiveSeed(SeedType::SEED_CHERRYBOMB));
    if (!sporeShellTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 4 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }

    const VSCardState *spore = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_SPORESHROOM);
    const int totalCost = spore == nullptr ? std::numeric_limits<int>::max() : PlantAIPlanning::EffectivePlantPlayCost(state, *spore);
    if (spore == nullptr || totalCost == std::numeric_limits<int>::max() || state.plantSun - totalCost < protectedSun) {
        return std::nullopt;
    }

    const int openingTarget = std::min(3, std::max(1, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 3));
    if (CountPlantType(state, SeedType::SEED_SPORESHROOM) >= openingTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_SPORESHROOM, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)) {
            continue;
        }

        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_SPORESHROOM, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_SPORESHROOM, target)) {
            continue;
        }

        const PlantLaneAssessment lane = AssessPlantLane(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        const VSZombieState *closest = FindClosestZombie(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_SPORESHROOM, row) * 7;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 12;
        score += closest == nullptr ? 45 : (closest->positionX > 620.0f ? 75 : -80);
        score += lane.danger < 120 ? 65 : -90;
        score += row == preferredRow ? 35 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_SPORESHROOM, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 || bestTarget.row < 0
        ? std::nullopt
        : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *spore, bestTarget, state.boardTick));
}

std::optional<VSAction> PlantAIPlanning::TryFumeDoomPressure(const VSGameState &state, int preferredRow, int protectedSun) {
    const auto HasActiveSeed = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };

    // In the Fume/Doom replay, Doom and Chilly are the one-shot release
    // valves. The actual board advantage comes from a compact Fume firing
    // line in columns two and three, built immediately after the initial
    // sun base instead of treating Sun-shroom padding as another producer.
    const bool fumeDoomTemplate = HasActiveSeed(SeedType::SEED_FUMESHROOM)
        && HasActiveSeed(SeedType::SEED_DOOMSHROOM) && HasActiveSeed(SeedType::SEED_INSTANT_COFFEE)
        && HasActiveSeed(SeedType::SEED_SUNSHROOM) && HasActiveSeed(SeedType::SEED_WALLNUT)
        && HasActiveSeed(SeedType::SEED_CHILLY_PEPPER);
    if (!fumeDoomTemplate || EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) < 6 || CountZombieEconomy(state) == 0) {
        return std::nullopt;
    }

    const VSCardState *fume = PlantAIPlanning::FindReadyCard(state, SeedType::SEED_FUMESHROOM);
    const int totalCost = fume == nullptr ? std::numeric_limits<int>::max() : PlantAIPlanning::EffectivePlantPlayCost(state, *fume);
    if (fume == nullptr || totalCost == std::numeric_limits<int>::max() || state.plantSun - totalCost < protectedSun) {
        return std::nullopt;
    }

    const int firingLineTarget = std::min(state.rows, std::max(2, EffectiveAIEconomyCount(VSSide::Plants, CountPlantIncome(state)) - 5));
    if (CountPlantType(state, SeedType::SEED_FUMESHROOM) >= firingLineTarget) {
        return std::nullopt;
    }

    VSGridPosition bestTarget{};
    int bestScore = std::numeric_limits<int>::min();
    for (int offset = 0; offset < state.rows; ++offset) {
        const int row = (preferredRow + offset) % state.rows;
        if (HasPlantTypeInRow(state, SeedType::SEED_FUMESHROOM, row) || PlantAIPlanning::ShouldYieldLaneToMower(state, row)) {
            continue;
        }

        const VSGridPosition target = PlantAIPlanning::FindSustainedOutputCell(state, SeedType::SEED_FUMESHROOM, row);
        if (target.col < 0 || target.row < 0 || !IsPlantPlacementSafe(state, SeedType::SEED_FUMESHROOM, target)) {
            continue;
        }

        const PlantLaneAssessment lane = AssessPlantLane(state, row);
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        const VSZombieState *closest = FindClosestZombie(state, row);
        int score = SeedEconomyPressureOpportunity(state, SeedType::SEED_FUMESHROOM, row) * 6;
        score += PlantEconomyValueInRow(state, row) * 2 + firepower.deficit * 14;
        score += closest == nullptr ? 50 : (closest->positionX > 560.0f ? 95 : -100);
        score += lane.danger < 120 ? 75 : -85;
        score += row == preferredRow ? 30 : 0;
        score += StrategyBonus(state, VSSide::Plants, SeedType::SEED_FUMESHROOM, row);
        if (bestTarget.col < 0 || score > bestScore) {
            bestTarget = target;
            bestScore = score;
        }
    }
    return bestTarget.col < 0 || bestTarget.row < 0
        ? std::nullopt
        : std::optional<VSAction>(MakePlayAction(VSSide::Plants, *fume, bestTarget, state.boardTick));
}

} // namespace vsai::detail
