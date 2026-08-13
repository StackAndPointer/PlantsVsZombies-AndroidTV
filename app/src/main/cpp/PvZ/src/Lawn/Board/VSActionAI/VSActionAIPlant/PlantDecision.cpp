#include "PlantAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include "PvZ/Lawn/Common/GameConstants.h"

namespace vsai::detail {

std::optional<VSAction> PlantAI::Decide(const VSGameState &state) {
    AdvanceBlockedSlots();
    // This is an action-order invariant, not an empty-board heuristic. The
    // first plant-side card must establish income before collection or any
    // replay/template branch is allowed to issue another PlaySeed.
    if (!state.isSuddenDeath && !mOpeningEconomyPlaced) {
        const SeedType openingSeed = state.isNight ? SeedType::SEED_SUNSHROOM : SeedType::SEED_SUNFLOWER;
        const VSCardState *card = PlantAIPlanning::FindReadyCard(state, openingSeed);
        const VSGridPosition target = FindSafeIncomeCell(state, LeastDevelopedPlantRow(state));
        if (card != nullptr && target.col >= 0 && target.row >= 0 && state.plantSun >= card->cost) {
            return MakePlayAction(VSSide::Plants, *card, target, state.boardTick);
        }
        return std::nullopt;
    }
    // A Jalapeno Head turns its first chew collision into a whole-row burn.
    // Moving the contacted front plant has to outrank resource collection.
    if (std::optional<VSAction> action = PlantAIPlanning::TryEvadeJalapenoHead(state)) {
        return action;
    }
    for (const VSResourceState &resource : state.resources) {
        if (resource.side == VSSide::Plants && !resource.dead && !resource.beingCollected) {
            return VSAction{.side = VSSide::Plants, .kind = VSActionKind::CollectResource, .objectId = resource.id, .sequence = ++mSequence};
        }
    }

    // A single Blover is the full-board answer to live Balloon Zombies.
    // Resolve it before lane-local counters, economy, or output choices.
    if (std::optional<VSAction> action = PlantAIPlanning::TryBlover(state, MostUrgentCounterRow(state))) {
        return action;
    }

    // A laddered nut no longer blocks the zombie route. Shovel it before
    // investing another card into the same lane so a fresh barrier can be
    // planted at a useful timing/column.
    if (std::optional<VSAction> action = PlantAIPlanning::TryRemoveLadderedNut(state)) {
        return action;
    }

    const PlantDecisionContext context = BuildDecisionContext(state);
    const PlantLaneAssessment &danger = context.danger;
    const PlantLaneAssessment &counterLane = context.counterLane;
    const PlantLaneFirepower &counterFirepower = context.counterFirepower;
    const PlantLaneFirepower &weakestFirepower = context.weakestFirepower;
    const int actualIncomePlantCount = context.actualIncomePlantCount;
    const int incomePlantCount = context.incomePlantCount;
    const int openingIncomeTarget = context.openingIncomeTarget;
    const int minimumIncomeBeforeOutput = context.minimumIncomeBeforeOutput;
    const int sustainedOutputCount = context.sustainedOutputCount;
    const int firepowerRow = context.firepowerRow;
    const int areaCounterReserve = context.areaCounterReserve;
    const int incomeExpansionTarget = context.incomeExpansionTarget;
    const int largestFirepowerDeficit = context.largestFirepowerDeficit;
    const int counterCombatPlants = context.counterCombatPlants;
    const int protectedSun = context.protectedSun;
    const int zombieEconomyStrikeRow = context.zombieEconomyStrikeRow;
    const bool economyZombieDeck = context.economyZombieDeck;
    const bool hasIncomeSeed = context.hasIncomeSeed;
    const bool hasSunshroomFiller = context.hasSunshroomFiller;
    const bool hasSustainedOutputSeed = context.hasSustainedOutputSeed;
    const bool hasActiveZombie = context.hasActiveZombie;
    const bool zombieCluster = context.zombieCluster;
    const bool midGame = context.midGame;
    const bool pressureOutrunsFirepower = context.pressureOutrunsFirepower;
    const bool immediateCounterThreat = context.immediateCounterThreat;
    const bool canStrikeZombieEconomy = context.canStrikeZombieEconomy;
    const bool needsSustainedOutput = context.needsSustainedOutput;
    const bool highSunCombatPressure = context.highSunCombatPressure;
    const bool openingNeedsFirepower = context.openingNeedsFirepower;
    const bool outputTempoHasPriority = context.outputTempoHasPriority;
    const bool mayExpandIncomePastOpening = context.mayExpandIncomePastOpening;
    const PlantDecisionResult emergency = TryEmergencyPolicy(state, context);
    if (emergency.handled) {
        return emergency.action;
    }
    if (openingNeedsFirepower) {
        if (hasSustainedOutputSeed) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, {.protectedSun = protectedSun, .allowLowCostCombat = true, .requirePreferredRow = true, .allowEmergencyTrade = true})) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryWakeableMushroomOutput(state, firepowerRow, protectedSun)) {
            return action;
        }
    }
    if (std::optional<VSAction> action = PlantAIPlanning::TryWakeSleepingMushroom(state, danger.row)) {
        return action;
    }
    // Template pressure begins only after the physical economy needed by the
    // selected main C exists. Emergency counters and lane defense above are
    // still allowed to interrupt this, but an otherwise safe board cannot
    // turn four Sunflowers into an early 125-Sun carry.
    const bool mustFundMainCarry = hasIncomeSeed && actualIncomePlantCount < openingIncomeTarget
        && !immediateCounterThreat && !openingNeedsFirepower && !pressureOutrunsFirepower
        && danger.danger < 145 && (counterFirepower.canHold || weakestFirepower.closestDistance > 760);
    if (mustFundMainCarry) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryIncomePlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
            return action;
        }
        return std::nullopt;
    }
    if (!immediateCounterThreat && !openingNeedsFirepower && CountZombieEconomy(state) > 0) {
        if (economyZombieDeck) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryGraveBuster(state, protectedSun)) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryMelonScaredySupport(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryScaredyCoffeeTempo(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryPeaPuffTempoOpening(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryPeaCeleryAshTempo(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TrySporePuffTempoPressure(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryPeaCabbageTorchTempo(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
    }
    if (!immediateCounterThreat && !openingNeedsFirepower) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryFumeDoomPressure(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TrySporeShellPressure(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryBoomerangControlPressure(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryBoomerangGarlicFormation(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryThreepeaterPuffFormation(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TrySnowpeaPuffMagnetPressure(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TrySnowpeaBonkFormation(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryPeaDoomTempoPressure(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryStarfruitCrossfireFormation(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryCactusSpikeweedCore(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryKernelCeleryFormation(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryRepeaterCeleryTempo(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryMelonMineTempo(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryRepeaterTempoPressure(state, zombieEconomyStrikeRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryMagnetShroom(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryScaredyMelonSupport(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryScaredyPuffDoomPressure(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (hasActiveZombie) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryStarfruitPuffPressure(state, firepowerRow, protectedSun)) {
                return action;
            }
            if (std::optional<VSAction> action = PlantAIPlanning::TryPeaPuffPressure(state, firepowerRow, protectedSun)) {
                return action;
            }
        }
    }
    if (hasActiveZombie && !immediateCounterThreat) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySporePuffPressure(state, firepowerRow, protectedSun)) {
            return action;
        }
    }
    // Puff-shroom is not the deck's primary ranged carry, but the new
    // replay uses Coffee-backed puffs to turn a short-range deficit into
    // cheap front-line fire before committing another expensive plant.
    if (hasActiveZombie && !immediateCounterThreat && (largestFirepowerDeficit > 0 || counterLane.danger >= 95)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryWakeableMushroomOutput(state, firepowerRow, protectedSun)) {
            return action;
        }
    }

    if (hasActiveZombie && !immediateCounterThreat) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryStarfruitGarlicFormation(state, protectedSun)) {
            return action;
        }
    }

    if (highSunCombatPressure && !immediateCounterThreat) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, {.protectedSun = protectedSun, .allowLowCostCombat = true})) {
            return action;
        }
    }

    // When substantial sun is already banked under a live threat, a
    // failed ideal placement must not turn into a no-op. Use a safe rear
    // firing position even if this lane is a poor long-term projectile
    // trade; immediate placement safety is still enforced by the helper.
    if (hasActiveZombie && !immediateCounterThreat && hasSustainedOutputSeed
        && state.plantSun - protectedSun >= 200 && (danger.danger >= 80 || largestFirepowerDeficit > 0)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, {.protectedSun = protectedSun, .allowLowCostCombat = true, .allowEmergencyTrade = true})) {
            return action;
        }
    }

    // A healthy Sunflower count is not a win condition by itself.  When
    // the opposing grave economy is exposed, convert the available tempo
    // into direct pressure before investing another turn in own income.
    if (canStrikeZombieEconomy) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryGraveBuster(state, protectedSun)) {
            return action;
        }
        if (hasSustainedOutputSeed) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, zombieEconomyStrikeRow, {.protectedSun = protectedSun})) {
                return action;
            }
        }
    }

    const bool daytimePadTemplate = !state.isNight && hasSunshroomFiller && !hasIncomeSeed;
    if (!state.isNight && hasSunshroomFiller && !immediateCounterThreat
        && (hasActiveZombie || daytimePadTemplate)
        && (incomePlantCount >= minimumIncomeBeforeOutput || sustainedOutputCount > 0 || daytimePadTemplate)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySunshroomFiller(state, danger.row, protectedSun)) {
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
        if (std::optional<VSAction> action = PlantAIPlanning::TryTorchwoodSupport(state, protectedSun)) {
            return action;
        }
    }

    const bool safeIncomeShortfall = incomePlantCount < openingIncomeTarget && danger.danger < 105 && !pressureOutrunsFirepower;
    if (hasIncomeSeed && incomePlantCount < openingIncomeTarget && danger.danger < 150
        && (!highSunCombatPressure || safeIncomeShortfall)) {
        if (incomePlantCount >= minimumIncomeBeforeOutput && needsSustainedOutput) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, LeastDevelopedPlantRow(state), {.protectedSun = protectedSun})) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryIncomePlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
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
    const bool canExpandIncome = mayExpandIncomePastOpening
        && (danger.danger < 105 || (counterCombatPlants > 0 && danger.danger < 140))
        && (counterFirepower.canHold || weakestFirepower.closestDistance > 760);
    if (hasIncomeSeed && hasActiveZombie && incomePlantCount < incomeExpansionTarget && !immediateCounterThreat && canExpandIncome
        && (!highSunCombatPressure || (midGame && incomePlantCount < incomeExpansionTarget)) && !outputTempoHasPriority) {
        if (needsSustainedOutput) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, {.protectedSun = protectedSun})) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryIncomePlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
            return action;
        }
    }

    if (!hasActiveZombie) {
        const int buildRow = LeastDevelopedPlantRow(state);
        if (hasIncomeSeed && incomePlantCount < minimumIncomeBeforeOutput) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryIncomePlant(state, buildRow, protectedSun)) {
                return action;
            }
        }
        if (needsSustainedOutput) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, buildRow, {.protectedSun = protectedSun})) {
                return action;
            }
        }
        if (hasIncomeSeed && incomePlantCount < incomeExpansionTarget
            && (!highSunCombatPressure || (midGame && incomePlantCount < incomeExpansionTarget))
            && !outputTempoHasPriority && mayExpandIncomePastOpening) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryIncomePlant(state, buildRow, protectedSun)) {
                return action;
            }
        }
        if (incomePlantCount >= 6 && sustainedOutputCount >= 3) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryTorchwoodSupport(state, protectedSun)) {
                return action;
            }
        }
        // Once the replay-like economy is established, pre-build only a
        // combat plant. Nuts and instant counters wait for a visible lane.
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, buildRow, {.protectedSun = protectedSun})) {
            return action;
        }
        return PlantAIPlanning::TryFallbackPlant(state, danger, buildRow);
    }

    const bool hasRangedHarasser = HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_PEA_HEAD)
        || HasZombieTypeInRow(state, danger.row, ZombieType::ZOMBIE_SUNDAY_EDITION);
    if (danger.danger >= 85 && (hasRangedHarasser || SustainedOutputScoreInRow(state, danger.row) >= 55)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryPumpkinShell(state, danger.row, protectedSun)) {
            return action;
        }
    }

    const bool yieldDangerLane = PlantAIPlanning::ShouldYieldLaneToMower(state, danger.row);
    if (danger.danger >= 105 && !yieldDangerLane) {
        if (!IsRangedOutputTradeUnfavorable(state, danger.row) && !HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, danger.row)) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryPlant(state, SeedType::SEED_SNOWPEA, danger.row, 1, 2)) {
                return action;
            }
        }
        if (!HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, danger.row)) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryPlant(state, SeedType::SEED_BONK_CHOY, danger.row, 3, 3)) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryPumpkinShell(state, danger.row, protectedSun)) {
            return action;
        }
        if (ShouldDeployWallnut(state, danger.row)) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryPlant(state, SeedType::SEED_WALLNUT, danger.row, 3, 5)) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TrySpikeweed(state, danger.row, protectedSun)) {
            return action;
        }
    }

    const int buildRow = LeastDevelopedPlantRow(state);
    if (!immediateCounterThreat && incomePlantCount >= 6 && sustainedOutputCount >= 3) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryTorchwoodSupport(state, protectedSun)) {
            return action;
        }
    }
    if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, buildRow, {.protectedSun = protectedSun})) {
        return action;
    }
    if (hasActiveZombie && !ShouldYieldLaneToMower(state, buildRow)
        && !HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, buildRow)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryPlant(state, SeedType::SEED_BONK_CHOY, buildRow, 3, 3)) {
            return action;
        }
    }
    if (!yieldDangerLane && PlantAIPlanning::ShouldDeployWallnut(state, danger.row)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryPlant(state, SeedType::SEED_WALLNUT, danger.row, 3, 5)) {
            return action;
        }
    }
    if (!yieldDangerLane) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySpikeweed(state, danger.row, protectedSun)) {
            return action;
        }
    }
    if (hasIncomeSeed && incomePlantCount < incomeExpansionTarget
        && (!highSunCombatPressure || (midGame && incomePlantCount < incomeExpansionTarget))
        && !outputTempoHasPriority && mayExpandIncomePastOpening) {
        return PlantAIPlanning::TryIncomePlant(state, buildRow, protectedSun);
    }
    // The safety-aware output helper is also the final escape hatch for a
    // rich, contested board. It refuses unsafe cells and preserves any
    // counter reserve, but it must get one last chance before this turn
    // becomes an unexplained no-op.
    if (hasActiveZombie && state.plantSun - protectedSun >= 125) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, {.protectedSun = protectedSun, .allowLowCostCombat = true, .allowEmergencyTrade = true})) {
            return action;
        }
    }
    return PlantAIPlanning::TryFallbackPlant(state, danger, buildRow);
}

} // namespace vsai::detail
