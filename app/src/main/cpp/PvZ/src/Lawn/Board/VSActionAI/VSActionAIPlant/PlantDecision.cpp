#include "PlantAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include "PvZ/Lawn/Common/GameConstants.h"

namespace vsai::detail {

std::optional<VSAction> PlantAI::Decide(const VSGameState &state) {
    AdvanceBlockedSlots();
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

    // A pressure template must never spend the first planting turn on a
    // shooter. Establish the map-appropriate income producer before any
    // formation, output, or grave-pressure branch is allowed to run.
    if (!state.isSuddenDeath && CountLivePlants(state) == 0) {
        const SeedType openingIncome = state.isNight ? SeedType::SEED_SUNSHROOM : SeedType::SEED_SUNFLOWER;
        const VSCardState *card = PlantAIPlanning::FindReadyCard(state, openingIncome);
        const VSGridPosition target = FindSafeIncomeCell(state, LeastDevelopedPlantRow(state));
        if (card != nullptr && target.col >= 0 && target.row >= 0 && state.plantSun >= card->cost) {
            return MakePlayAction(VSSide::Plants, *card, target, state.boardTick);
        }
        return std::nullopt;
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

    const PlantLaneAssessment danger = MostThreatenedPlantLane(state);
    const std::uint16_t zombieDeckArchetype = DeckArchetype(state, VSSide::Zombies);
    const bool fastZombieDeck = (zombieDeckArchetype & kZombieDeckFastPressure) != 0;
    const bool metalZombieDeck = (zombieDeckArchetype & kZombieDeckMetalScreen) != 0;
    const bool vehicleZombieDeck = (zombieDeckArchetype & kZombieDeckVehicle) != 0;
    const bool economyZombieDeck = (zombieDeckArchetype & kZombieDeckEconomy) != 0;
    const bool rangedZombieDeck = (zombieDeckArchetype & kZombieDeckRangedSiege) != 0;
    const bool heavyZombieDeck = (zombieDeckArchetype & kZombieDeckHeavy) != 0;
    const bool swarmZombieDeck = (zombieDeckArchetype & kZombieDeckSwarm) != 0;
    const bool deckNeedsEarlyFirepower = fastZombieDeck || vehicleZombieDeck || rangedZombieDeck || heavyZombieDeck || swarmZombieDeck;
    // Five Sunflowers already fund the low-cost pressure seen in VS. Do not
    // hold a healthy board at seven merely because more income slots exist.
    const int baseOpeningIncomeTarget = std::max(3, state.rows >= 6 ? 5 : 4);
    const int openingIncomeTarget = deckNeedsEarlyFirepower
        ? std::max(3, baseOpeningIncomeTarget - 1)
        : baseOpeningIncomeTarget;
    const int minimumIncomeBeforeOutput = std::max(2, deckNeedsEarlyFirepower ? 3 : (state.rows >= 6 ? 4 : 3));
    const int actualIncomePlantCount = CountPlantIncome(state);
    const int primaryOutputCost = PlantAIPlanning::PrimaryOutputCost(state);
    // Enhanced AI advances cheap pressure by one economy step. A costly main
    // still needs its physical producers: treating five real Sunflowers as
    // six for a Melon/Coffee timing strands the AI below its first carry.
    const bool enhancedCheapCarry = vsai::IsEnhancedAIEnabled() && vsai::IsSideEnabled(VSSide::Plants)
        && primaryOutputCost > 0 && primaryOutputCost < 150;
    const int incomePlantCount = actualIncomePlantCount + (enhancedCheapCarry ? 1 : 0);
    const int sustainedOutputCount = CountSustainedOutputPlants(state);
    const bool hasIncomeSeed = PlantAIPlanning::HasIncomeSeed(state);
    const bool hasSunshroomFiller = PlantAIPlanning::HasSunshroomSeed(state);
    const bool hasSustainedOutputSeed = HasSustainedOutputSeed(state);
    const bool hasActiveZombie = CountActiveZombies(state) > 0;
    const int counterRow = MostUrgentCounterRow(state);
    const bool mowerlessThirdColumnEmergency = IsMowerlessThirdColumnEmergency(state, counterRow);
    const VSZombieState *counterClosest = FindClosestZombie(state, counterRow);
    const int counterZombieCount = CountZombiesInRow(state, counterRow);
    const int counterStackCount = PlantAIPlanning::LargestSquashTargetStackInRow(state, counterRow);
    const PlantLaneAssessment counterLane = AssessPlantLane(state, counterRow);
    const PlantLaneFirepower counterFirepower = AssessPlantLaneFirepower(state, counterRow);
    int firepowerRow = counterRow;
    int largestFirepowerDeficit = counterFirepower.deficit;
    int contestedZombieRows = 0;
    int unholdableZombieRows = 0;
    int incomingZombieHealth = 0;
    int damageBeforeZombieContact = 0;
    for (int row = 0; row < state.rows; ++row) {
        const PlantLaneFirepower firepower = AssessPlantLaneFirepower(state, row);
        if (firepower.deficit > largestFirepowerDeficit
            || (firepower.deficit == largestFirepowerDeficit && !firepower.canHold && row != counterRow)) {
            firepowerRow = row;
            largestFirepowerDeficit = firepower.deficit;
        }
        if (firepower.incomingHealth <= 0) {
            continue;
        }
        ++contestedZombieRows;
        incomingZombieHealth += firepower.incomingHealth;
        damageBeforeZombieContact += firepower.damageBeforeContact;
        if (!firepower.canHold || firepower.deficit > 0) {
            ++unholdableZombieRows;
        }
    }
    const PlantLaneFirepower weakestFirepower = AssessPlantLaneFirepower(state, firepowerRow);
    const int counterCombatPlants = static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [counterRow](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && plant.position.row == counterRow && IsPlantCombatSeed(plant.seedType);
    }));
    const int combatPlantCount = static_cast<int>(std::count_if(state.plants.begin(), state.plants.end(), [](const VSPlantState &plant) {
        return !IsDeadOrOutside(plant) && IsPlantCombatSeed(plant.seedType);
    }));
    const bool hasCombatSeed = std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [](const VSCardState &card) {
        return card.active && !card.matchRestricted && IsPlantCombatSeed(card.seedType);
    });
    const bool hasGargantuar = HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_GARGANTUAR)
        || HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_GIGA_GARGANTUAR);
    const bool hasGigaPoleVaulter = HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_GIGA_POLEVAULTER);
    int zamboniRow = -1;
    int impactThreatRow = -1;
    float closestZamboniX = std::numeric_limits<float>::max();
    float closestImpactThreatX = std::numeric_limits<float>::max();
    for (const VSZombieState &zombie : state.zombies) {
        if (zombie.dead) {
            continue;
        }
        if (zombie.zombieType == static_cast<std::uint16_t>(ZombieType::ZOMBIE_ZAMBONI) && zombie.positionX < closestZamboniX) {
            zamboniRow = zombie.row;
            closestZamboniX = zombie.positionX;
        }
        const ZombieType zombieType = static_cast<ZombieType>(zombie.zombieType);
        if ((zombieType == ZombieType::ZOMBIE_SQUASH_HEAD || zombieType == ZombieType::ZOMBIE_GIGA_FOOTBALL)
            && zombie.positionX < closestImpactThreatX) {
            impactThreatRow = zombie.row;
            closestImpactThreatX = zombie.positionX;
        }
    }
    const bool earlySingleBucket = state.boardTick < 32000 && counterZombieCount == 1
        && HasZombieTypeInRow(state, counterRow, ZombieType::ZOMBIE_PAIL) && counterCombatPlants == 0 && counterLane.plantCount <= 2
        && counterClosest != nullptr && counterClosest->positionX < 700.0f && !counterFirepower.canHold;
    // Squash is an area/tempo card. The full-board target evaluator below
    // owns its normal cluster and durable-body decisions; retain only the
    // early isolated Pail exception here.
    const bool zombieCluster = hasActiveZombie && counterStackCount >= 2;
    const bool squashThreat = earlySingleBucket;
    const bool impPearThreat = (hasGargantuar || hasGigaPoleVaulter) && counterClosest != nullptr
        && (counterClosest->eating || counterClosest->positionX < 780.0f || counterLane.danger >= 160);
    const int counterFirstColumn = mowerlessThirdColumnEmergency ? 0 : 4;
    const int areaCounterReserve = PlantAIPlanning::AreaCounterReserve(state);
    const bool hasEconomyPressurePlan = PlantAIPlanning::HasEconomyPressurePlan(state);
    const bool midGame = state.boardTick >= 32000 || CountZombieEconomy(state) >= state.rows;
    const int normalIncomeBase = std::max(3, state.rows >= 6 ? 5 : 4);
    // Enhanced tempo advances pressure thresholds, not the physical plant
    // count. Restore the real producer line in midgame so accelerated
    // openings do not remain starved after early trades.
    const int enhancedIncomeRecoveryTarget = normalIncomeBase
        + (vsai::IsEnhancedAIEnabled() && vsai::IsSideEnabled(VSSide::Plants) ? 1 : 0);
    const int lateIncomeRecoveryTarget = !state.isSuddenDeath && midGame && actualIncomePlantCount < enhancedIncomeRecoveryTarget
        ? enhancedIncomeRecoveryTarget
        : 0;
    const int economyPressureIncomeTarget = PlantAIPlanning::EconomyPressureIncomeTarget(state);
    const int incomeExpansionTarget = state.isSuddenDeath ? 0
        : std::max(economyPressureIncomeTarget, lateIncomeRecoveryTarget);
    const bool pressureOutrunsFirepower = hasActiveZombie && (unholdableZombieRows > 0
        || (contestedZombieRows >= 2 && damageBeforeZombieContact < incomingZombieHealth));
    const bool immediateCounterThreat = squashThreat || impPearThreat || mowerlessThirdColumnEmergency;
    const bool mustHoldCounterReserve = areaCounterReserve > 0 && state.plantSun >= areaCounterReserve
        && (HasReadyZombieBreakthroughCard(state) || ((heavyZombieDeck || swarmZombieDeck) && hasActiveZombie));
    const int protectedSun = mustHoldCounterReserve ? areaCounterReserve : 0;
    const int zombieEconomyStrikeRow = MostVulnerableZombieEconomyRow(state);
    const int economyStrikeIncomeFloor = economyZombieDeck ? std::max(2, minimumIncomeBeforeOutput - 1) : minimumIncomeBeforeOutput;
    const bool canStrikeZombieEconomy = (state.isSuddenDeath || incomePlantCount >= economyStrikeIncomeFloor) && hasEconomyPressurePlan
        && CountZombieEconomy(state) > 0 && danger.danger < 150 && !immediateCounterThreat;
    const bool hasSporeCarry = std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [](const VSCardState &card) {
        return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_SPORESHROOM);
    });
    // Winning replay boards do not stop at one attacker per lane. They
    // first establish a compact sun base, then turn surplus sun into a
    // second firing layer on the lanes that can hold it.
    const int maximumOutputCount = std::max(1, state.rows * (hasSporeCarry ? 3 : 2));
    int desiredOutputCount = state.isSuddenDeath
        ? maximumOutputCount
        : std::min(maximumOutputCount, std::max(1, (incomePlantCount + 1) / 2));
    if (!state.isSuddenDeath && incomePlantCount >= openingIncomeTarget) {
        desiredOutputCount = std::max(desiredOutputCount, std::min(maximumOutputCount, state.rows + 1));
    }
    if (!state.isSuddenDeath && state.plantSun >= 125) {
        const int reserveOutputTarget = state.rows + (state.plantSun >= 350 ? 2 : 1);
        desiredOutputCount = std::max(desiredOutputCount, std::min(maximumOutputCount, reserveOutputTarget));
    }
    if (!state.isSuddenDeath && deckNeedsEarlyFirepower && incomePlantCount >= minimumIncomeBeforeOutput) {
        // A fast/vehicle/ranged/giant deck gets a firing layer one producer
        // earlier than the neutral replay prior. This is the main matchup
        // distinction that the old template-only policy was missing.
        desiredOutputCount = std::max(desiredOutputCount, std::min(maximumOutputCount, state.rows));
    }
    if (!state.isSuddenDeath && hasSporeCarry) {
        // The Spore/Puff/Coffee replay wins by adding layers of the same
        // lobbed carry after its compact sun opening. It is still one
        // main damage card, but it needs a deeper firing band than an
        // expensive Melon-pult or Repeater deck.
        const int sporeLayerTarget = state.plantSun >= 500 ? maximumOutputCount
            : (state.plantSun >= 350 ? state.rows * 2 + 2 : state.rows + (state.plantSun >= 200 ? 3 : 1));
        desiredOutputCount = std::max(desiredOutputCount, std::min(maximumOutputCount, sporeLayerTarget));
    }
    const bool needsSustainedOutput = hasSustainedOutputSeed
        && (sustainedOutputCount < desiredOutputCount || (hasActiveZombie && largestFirepowerDeficit > 0));
    const int highSunCombatTarget = std::min(maximumOutputCount, state.rows + (state.plantSun >= 350 ? 2 : 1));
    const bool highSunCombatPressure = !state.isSuddenDeath && state.plantSun >= 125
        && incomePlantCount >= minimumIncomeBeforeOutput && (hasCombatSeed || hasSustainedOutputSeed)
        && (combatPlantCount < highSunCombatTarget || needsSustainedOutput || largestFirepowerDeficit > 0);
    const bool readySustainedOutput = PlantAIPlanning::HasReadySustainedOutputCard(state, protectedSun);
    // A replay-style grave pressure opening still has to stop the first
    // live route. Do not turn a firepower deficit into an economy strike.
    const bool deckOpeningPressure = deckNeedsEarlyFirepower && !hasActiveZombie
        && incomePlantCount >= minimumIncomeBeforeOutput && state.plantSun >= 100;
    const bool openingNeedsFirepower = deckOpeningPressure || (hasActiveZombie && (counterFirepower.deficit > 0 || !counterFirepower.canHold
        || (counterClosest != nullptr && counterClosest->positionX < 720.0f && counterCombatPlants == 0)));
    // Five Sunflowers are a compact opening base, not a permanent command to
    // stop farming.  Once the match is in its grave-economy phase, however,
    // every contested lane must be able to remove its incoming health before
    // contact before another Sunflower is considered.  This keeps the
    // decision tied to real lane DPS and zombie health rather than a fixed
    // producer count.
    const bool outputTempoHasPriority = incomePlantCount >= openingIncomeTarget && needsSustainedOutput
        && readySustainedOutput && (pressureOutrunsFirepower || midGame);
    const bool mayExpandIncomePastOpening = incomePlantCount < openingIncomeTarget || !midGame
        || (!pressureOutrunsFirepower && (!needsSustainedOutput || !readySustainedOutput));
    // Enhanced AI still needs a real late economy. Once every live lane can
    // hold its current wave, recover missing producers even when a high-sun
    // output branch is available; otherwise a cheap early push leaves it at
    // three or four producers for the remainder of the match.
    const bool canRecoverLateIncome = lateIncomeRecoveryTarget > 0 && !immediateCounterThreat
        && !pressureOutrunsFirepower && danger.danger < 145
        && (counterFirepower.canHold || weakestFirepower.closestDistance > 760)
        && (!openingNeedsFirepower || actualIncomePlantCount < normalIncomeBase);

    // The recorded plant side builds its sun base first, then answers a real
    // heavy/fast push with Squash. It is never an opening filler card.
    // Against Gargantuars the replay preserves Imp Pear for the first
    // answer; Squash is the follow-up when the giant reaches the line.
    if (zamboniRow >= 0) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySpikeweed(state, zamboniRow, protectedSun)) {
            return action;
        }
    }
    if (impactThreatRow >= 0) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryImpactDistraction(state, impactThreatRow, protectedSun)) {
            return action;
        }
    }
    if (hasActiveZombie) {
        // Every Ash card chooses its own full-board best legal target.
        // This is deliberately before normal economy/output spending: a
        // genuine kill cluster must not lose its timing to a filler move.
        if (std::optional<VSAction> action = PlantAIPlanning::TryWakeSleepingDoomshroom(state)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryAshCounter(state, SeedType::SEED_DOOMSHROOM, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryAshCounter(state, SeedType::SEED_CHERRYBOMB, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryAshCounter(state, SeedType::SEED_JALAPENO, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryAshCounter(state, SeedType::SEED_SQUASH, protectedSun)) {
            return action;
        }
        // Chilly Pepper needs one second before it damages its row. Resolve
        // a legal immediate Squash first; otherwise Squash can clear the
        // exact cluster Chilly selected and leave its delayed blast empty.
        if (std::optional<VSAction> action = PlantAIPlanning::TryAshCounter(state, SeedType::SEED_CHILLY_PEPPER, protectedSun)) {
            return action;
        }
    }
    if (std::optional<VSAction> action = PlantAIPlanning::TryUmbrellaDefense(state, protectedSun)) {
        return action;
    }
    if (hasActiveZombie && !mowerlessThirdColumnEmergency) {
        // Hypno-shroom is a conversion card, not generic mushroom filler:
        // feed it a durable threat that can walk back into nearby zombies.
        if (std::optional<VSAction> action = PlantAIPlanning::TryHypnoshroom(state, counterRow, protectedSun)) {
            return action;
        }
        // Mines are only deployed while their target has enough runway to
        // arm. Immediate pressure is handled by Ash, Iceberg, or a wall.
        if (std::optional<VSAction> action = PlantAIPlanning::TryPotatoMine(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (metalZombieDeck) {
            // Magnet is a matchup answer, not a late template ornament. Once
            // an actual metal screen enters a lane, resolve it before adding
            // another direct-fire plant that the screen hard-counters.
            if (std::optional<VSAction> action = PlantAIPlanning::TryMagnetShroom(state, firepowerRow, protectedSun)) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TrySnowpeaBonkPressure(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryStarfruitChomperPressure(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryKernelCeleryPressure(state, firepowerRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryCactusSpikeweedPressure(state, firepowerRow, protectedSun)) {
            return action;
        }
    }
    if (hasActiveZombie && impPearThreat && !HasPlantTypeInRow(state, SeedType::SEED_IMP_PEAR, counterRow)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryCounterPlant(state, SeedType::SEED_IMP_PEAR, counterRow, counterFirstColumn)) {
            return action;
        }
    }
    if (squashThreat && counterClosest != nullptr && !HasPlantTypeInRow(state, SeedType::SEED_SQUASH, counterRow)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryCounterPlant(state, SeedType::SEED_SQUASH, counterRow, counterFirstColumn)) {
            return action;
        }
    }
    if (hasActiveZombie) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryIcebergLettuce(state, counterRow, protectedSun, mowerlessThirdColumnEmergency)) {
            return action;
        }
        if (rangedZombieDeck) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryPumpkinShell(state, counterRow, protectedSun)) {
                return action;
            }
        }
        // Iceberg Lettuce is deliberately played at the zombie's feet.
        // Once it has bought that time, a Wall-nut is the next defensive
        // investment, ahead of another income or output decision.
        if (HasPlantTypeInRow(state, SeedType::SEED_ICEBERG_LETTUCE, counterRow)
            && PlantAIPlanning::ShouldDeployWallnut(state, counterRow)) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryPlant(state, SeedType::SEED_WALLNUT, counterRow, 3, 5)) {
                return action;
            }
        }
    }
    if (mowerlessThirdColumnEmergency) {
        // With no mower left, do not spend this turn on income, grave
        // pressure, or another lane.  A nut may be planted in the
        // zombie's current cell and is preferable to losing the row.
        if (ShouldDeployWallnut(state, counterRow)) {
            if (std::optional<VSAction> action = PlantAIPlanning::TryPlant(state, SeedType::SEED_WALLNUT, counterRow, 0, 5)) {
                return action;
            }
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TryPumpkinShell(state, counterRow, protectedSun)) {
            return action;
        }
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, counterRow, protectedSun, true, true, true)) {
            return action;
        }
        return std::nullopt;
    }
    if (openingNeedsFirepower) {
        if (hasSustainedOutputSeed) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, protectedSun, true, true, true)) {
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
    // An early pressure opening may deliberately pause at three producers,
    // but it must rebuild to the normal four/five-producer base once the
    // game reaches a stable midgame.  This is intentionally below immediate
    // defense and above optional replay formations.
    if (canRecoverLateIncome) {
        if (std::optional<VSAction> action = PlantAIPlanning::TryIncomePlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
            return action;
        }
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
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, protectedSun, true)) {
            return action;
        }
    }

    // When substantial sun is already banked under a live threat, a
    // failed ideal placement must not turn into a no-op. Use a safe rear
    // firing position even if this lane is a poor long-term projectile
    // trade; immediate placement safety is still enforced by the helper.
    if (hasActiveZombie && !immediateCounterThreat && hasSustainedOutputSeed
        && state.plantSun - protectedSun >= 200 && (danger.danger >= 80 || largestFirepowerDeficit > 0)) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, protectedSun, true, false, true)) {
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
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, zombieEconomyStrikeRow, protectedSun)) {
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

    if (hasIncomeSeed && incomePlantCount < openingIncomeTarget && danger.danger < 150 && !highSunCombatPressure) {
        if (incomePlantCount >= minimumIncomeBeforeOutput && needsSustainedOutput) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, LeastDevelopedPlantRow(state), protectedSun)) {
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
        && !highSunCombatPressure && !outputTempoHasPriority) {
        if (needsSustainedOutput) {
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, protectedSun)) {
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
            if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, buildRow, protectedSun)) {
                return action;
            }
        }
        if (hasIncomeSeed && incomePlantCount < incomeExpansionTarget && !highSunCombatPressure
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
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, buildRow, protectedSun)) {
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
    if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, buildRow, protectedSun)) {
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
    if (hasIncomeSeed && incomePlantCount < incomeExpansionTarget && !highSunCombatPressure
        && !outputTempoHasPriority && mayExpandIncomePastOpening) {
        return PlantAIPlanning::TryIncomePlant(state, buildRow, protectedSun);
    }
    // The safety-aware output helper is also the final escape hatch for a
    // rich, contested board. It refuses unsafe cells and preserves any
    // counter reserve, but it must get one last chance before this turn
    // becomes an unexplained no-op.
    if (hasActiveZombie && state.plantSun - protectedSun >= 125) {
        if (std::optional<VSAction> action = PlantAIPlanning::TrySustainedOutputPlant(state, firepowerRow, protectedSun, true, false, true)) {
            return action;
        }
    }
    return PlantAIPlanning::TryFallbackPlant(state, danger, buildRow);
}

} // namespace vsai::detail
