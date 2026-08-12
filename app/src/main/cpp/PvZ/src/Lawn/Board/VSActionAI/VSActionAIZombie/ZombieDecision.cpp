#include "ZombieAI.h"

#include "../VSActionAILanePolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace vsai::detail {

void ZombieAIPlanning::Reset() {
    BuiltinVSAgent::Reset();
    mLastAttackRow = -1;
    mLastPressureEconomyCount = -1;
    mLaneAttackCooldown.fill(0);
}

std::optional<VSAction> ZombieAI::Decide(const VSGameState &state) {
    AdvanceBlockedSlots();
    for (std::uint8_t &cooldown : mLaneAttackCooldown) {
        if (cooldown > 0) {
            --cooldown;
        }
    }
    for (const VSResourceState &resource : state.resources) {
        if (resource.side == VSSide::Zombies && !resource.dead && !resource.beingCollected) {
            return VSAction{.side = VSSide::Zombies, .kind = VSActionKind::CollectResource, .objectId = resource.id, .sequence = ++mSequence};
        }
    }

    const ZombieDecisionContext context = BuildZombieDecisionContext(state);
    const int actualEconomyCount = context.actualEconomyCount;
    if (mLastPressureEconomyCount > actualEconomyCount) {
        // A destroyed grave re-opens the pressure cadence. Rebuilding
        // from a smaller base should not force a full 15-grave rebuild
        // before the next low-cost probe is allowed.
        mLastPressureEconomyCount = actualEconomyCount - 1;
    }
    const ZombieTempoPolicy &tempo = context.tempo;
    const int economyCount = context.economyCount;
    const int economyDeficit = context.economyDeficit;
    int targetMarkersOnBoard = 0;
    int zeroHealthTargetMarkers = 0;
    for (const VSGridItemState &item : state.gridItems) {
        if (item.gridItemType != static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_TARGET_ZOMBIE)) {
            continue;
        }
        ++targetMarkersOnBoard;
        // A target which has reached zero health remains on the board for a
        // death animation. A mDead item has already been removed from the
        // board-owned live count, so do not count it a second time here.
        zeroHealthTargetMarkers += !item.dead && item.health <= 0 ? 1 : 0;
    }
    // A third destroyed target loses VS outright. Once two are gone, the
    // remaining live target lanes take priority over normal economy tempo.
    // A target stays in a zero-health death animation before GridItemDie, so
    // combine that immediate signal with Board's count after old markers have
    // already been released from the grid-item array.
    const int historicalTargetLosses = std::max(0, state.rows - state.liveZombieTargetCount);
    const bool targetDefenseEmergency = targetMarkersOnBoard > 0
        && historicalTargetLosses + zeroHealthTargetMarkers >= 2;
    int criticalTargetRow = -1;
    if (targetDefenseEmergency) {
        int finalTargetThreat = std::numeric_limits<int>::min();
        for (const VSGridItemState &item : state.gridItems) {
            if (item.gridItemType != static_cast<std::uint16_t>(GridItemType::GRIDITEM_MP_TARGET_ZOMBIE)
                || item.dead || item.health <= 0 || item.position.row < 0 || item.position.row >= state.rows) {
                continue;
            }
            const int row = item.position.row;
            const int threat = ProtectableGraveThreatScore(state, row) * 3
                + ZombieGraveScreenDeficit(state, row) * 2 + ZombieFrontlineValueInRow(state, row);
            if (criticalTargetRow < 0 || threat > finalTargetThreat) {
                criticalTargetRow = row;
                finalTargetThreat = threat;
            }
        }
    }
    // After two targets fall, losing any further live target ends the match.
    // Defend the most threatened remaining target route before ordinary
    // economy tempo, without treating the other live target routes as lost.
    const int graveDefenseRow = criticalTargetRow >= 0 ? criticalTargetRow : MostThreatenedEconomyRow(state);
    const int graveDefenseScore = ProtectableGraveThreatScore(state, graveDefenseRow);
    const int graveStraightThreat = StraightProjectileThreatScore(state, graveDefenseRow);
    const int graveLobbedThreat = LobbedProjectileThreatScore(state, graveDefenseRow);
    const int graveScreenDeficit = ZombieGraveScreenDeficit(state, graveDefenseRow);
    const bool hasGraveGuard = HasZombieGraveGuardInRow(state, graveDefenseRow);
    const bool proactiveGraveScreen = NeedsProactiveGraveScreen(state, graveDefenseRow);
    const bool graveDefenseUrgent = targetDefenseEmergency || graveDefenseScore >= 50 || graveStraightThreat >= 55
        || graveLobbedThreat >= 70 || graveScreenDeficit >= 55 || proactiveGraveScreen;
    const bool graveDefenseReinforcement = graveDefenseUrgent && (targetDefenseEmergency || !hasGraveGuard || graveScreenDeficit >= 55);
    if (targetDefenseEmergency) {
        if (std::optional<VSAction> action = ZombieAIPlanning::TryProtectEconomy(state, graveDefenseRow, true)) {
            return action;
        }
    }
    if (graveLobbedThreat >= 70 && graveStraightThreat < 70) {
        if (std::optional<VSAction> action = ZombieAIPlanning::TryCounterLobbedGravePressure(state, context, graveDefenseRow)) {
            return action;
        }
    }
    if (graveDefenseReinforcement) {
        if (std::optional<VSAction> action = ZombieAIPlanning::TryProtectEconomy(state, graveDefenseRow, targetDefenseEmergency)) {
            return action;
        }
    }
    const int activePressureRows = context.activePressureRows;
    const int heavyZombieReserve = ZombieAIPlanning::HeavyZombieReserve(state);
    const int heavyEconomyThreshold = context.heavyEconomyThreshold;
    // High-cost cards are a deliberate conversion of a developed grave
    // economy, not an opening all-in.  Start banking before the final two
    // graves only when multiple routes already tax the plant player.
    const bool bankForHeavy = heavyZombieReserve >= 100
        && economyCount >= tempo.HeavyBankEconomyThreshold(state.rows, heavyEconomyThreshold)
        && tempo.HasAttackCommitPressure(activePressureRows, 2, state.rows)
        && CountLivePlants(state) >= state.rows && graveDefenseScore < 100;
    const int minimumOpeningEconomy = tempo.OpeningEconomyFloor(std::min(2, std::max(1, state.rows)));
    const int desiredOpeningRows = tempo.OpeningPressureRowTarget(std::min(3, state.rows), state.rows);
    const bool hasReadyFrontlineProbe = ZombieAIPlanning::HasReadyFrontlineProbe(state);
    const bool hasReadyEarlyHeavyCommit = ZombieAIPlanning::HasReadyEarlyHeavyCommit(state, context);
    if (std::optional<VSAction> action = ZombieAIPlanning::TryTemplateSundayRelease(state, context)) {
        return action;
    }
    bool canConvertMowerlessTargetRoute = false;
    for (int row = 0; row < state.rows; ++row) {
        if (EvaluateZombieLanePolicy(state, row).conversionRoute) {
            canConvertMowerlessTargetRoute = true;
            break;
        }
    }
    // The Normal/Trashcan/Dog/Football/Giant replay opens with a single
    // Normal immediately after its first grave. That cheap probe forces a
    // response while the later Trashcan still has an economy worth guarding;
    // it is not the same as a generic all-in after one grave.
    const ZombieTemplateProfile &profile = context.templateProfile;
    const bool armoredNormalRushTemplate = profile.Has(ZombieTemplate::ArmoredNormalRush);
    const bool impPailSundayTemplate = profile.Has(ZombieTemplate::ImpSledSunday);
    const bool zamboniPoleOpeningTemplate = profile.Has(ZombieTemplate::ZamboniPole);
    // Both recorded lines establish three rear graves before their first
    // Imp/Zomboni probe. Two graves give neither the later Sunday release
    // nor a returned Zomboni enough economy to stay on the board.
    const int openingPressureEconomyFloor = (impPailSundayTemplate || zamboniPoleOpeningTemplate)
        ? tempo.OpeningEconomyFloor(std::min(3, state.rows))
        : minimumOpeningEconomy;
    const bool firstGraveProbe = armoredNormalRushTemplate && actualEconomyCount == 1 && activePressureRows == 0
        && hasReadyFrontlineProbe && mLastPressureEconomyCount < actualEconomyCount;
    // Winning zombie replays establish a few rear graves, then alternate
    // a probe with another grave. One uninterrupted build to 15 gives the
    // plant side a free economic opening and never creates a threat lane.
    const int openingPressureEconomyCeiling = tempo.OpeningEconomyCeiling(state.rows + 1);
    const bool openingPressureCadence = economyCount >= openingPressureEconomyFloor
        && economyCount <= openingPressureEconomyCeiling && activePressureRows < desiredOpeningRows
        && mLastPressureEconomyCount < actualEconomyCount;
    const bool enhancedPressureRecovery = tempo.ShouldExtendPressure(economyCount, activePressureRows, state.rows);
    const bool forceOpeningPressure = firstGraveProbe || (hasReadyFrontlineProbe
        && (openingPressureCadence || enhancedPressureRecovery));
    const bool preservePressureDuringRepair = economyCount >= minimumOpeningEconomy
        && economyDeficit <= tempo.PressureRepairDeficitTolerance()
        && activePressureRows > 0 && hasReadyFrontlineProbe;
    const int survivingFrontRow = criticalTargetRow >= 0 ? criticalTargetRow : MostValuableZombieFrontRow(state);
    const int survivingFrontValue = ZombieFrontlineValueInRow(state, survivingFrontRow);
    const bool preserveSurvivingFront = criticalTargetRow >= 0
        || (economyCount >= state.rows && activePressureRows == 1 && survivingFrontValue >= 90);
    const bool survivingFrontGuarded = HasZombieGraveGuardInRow(state, survivingFrontRow);
    const int economicRow = economyCount < state.rows * 2 ? ZombieAIPlanning::LeastCommittedZombieRow(state) : LeastThreatenedEconomyRow(state);
    const bool restorationCanProceed = !graveDefenseReinforcement || hasGraveGuard;
    const bool restorationOutweighsFront = economyDeficit >= 2 || graveDefenseScore < 100 || hasGraveGuard;
    const bool economyRepairIsUrgent = economyCount < minimumOpeningEconomy
        || economyDeficit >= tempo.EconomyRepairDeficitThreshold()
        // Enhanced AI must restore a cleared midgame grave line instead of
        // permanently preferring pressure once it has a nominal lead.
        || (tempo.IsEnhanced() && economyDeficit > 0 && activePressureRows >= std::min(2, state.rows));
    const bool hasReadyTemplateCommit = HasReadyZombieTemplateCommit(state, context.templateProfile, context.tempo,
        context.actualEconomyCount, context.activePressureRows);
    if (economyDeficit > 0 && restorationCanProceed && !forceOpeningPressure
        && (!canConvertMowerlessTargetRoute || !hasReadyFrontlineProbe)
        && !hasReadyEarlyHeavyCommit && !hasReadyTemplateCommit
        && (!bankForHeavy || (tempo.IsEnhanced() && economyDeficit >= 2))
        && (economyRepairIsUrgent || !preservePressureDuringRepair) && (!preserveSurvivingFront || restorationOutweighsFront)) {
        if (std::optional<VSAction> action = ZombieAIPlanning::TryBuildEconomy(state, economicRow)) {
            return action;
        }
    }

    const bool saveForHeavy = bankForHeavy && state.zombieBrains < heavyZombieReserve;

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
        if (seed == SeedType::SEED_ZOMBIE_CATAPULT && HasMindControlledZombieInRow(state, row)) {
            // A hypnotized zombie turns the catapult lane into friendly
            // fire. Let that lane resolve before adding a ranged unit.
            return std::nullopt;
        }
        // A thrown projectile passes a slow metal screen rather than
        // trading into it. Spore-shroom uses the same trajectory class.
        const bool hasLobbedPlant = ZombieAIPlanning::HasLobbedPlantInRow(state, row);
        if (hasLobbedPlant && IsZombieLobbedScreenDonation(seed)) {
            return std::nullopt;
        }
        if (seed == SeedType::SEED_ZOMBIE_TRASHCAN || seed == SeedType::SEED_ZOMBIE_TALLNUT_HEAD) {
            // Trashcan advances too slowly to be an attacking probe. Its
            // job is to absorb direct fire before it reaches a grave;
            // Tall-nut Head takes the same role in mound decks.
            const int screenDeficit = ZombieGraveScreenDeficit(state, row);
            if ((!NeedsProactiveGraveScreen(state, row) && ProtectableGraveThreatScore(state, row) < 100
                 && StraightProjectileThreatScore(state, row) < 100 && screenDeficit < 120)
                || (HasZombieGraveGuardInRow(state, row) && screenDeficit < 120)) {
                return std::nullopt;
            }
        }
        if (IsZombieTargetedSeed(seed)) {
            const VSPlantState *targetPlant = nullptr;
            int targetScore = std::numeric_limits<int>::min();
            for (const VSPlantState &plant : state.plants) {
                if (IsDeadOrOutside(plant) || plant.position.row != row) {
                    continue;
                }
                const int plantScore = ZombieAIPlanning::BungeeTargetScore(state, plant, row);
                if (plantScore == std::numeric_limits<int>::min()) {
                    continue;
                }
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
    const bool plantHasReadyAsh = ReadyPlantAreaCounterCount(state) > 0;
    int unpressuredEconomyRows = 0;
    for (int row = 0; row < state.rows; ++row) {
        const ZombieLanePolicy lane = EvaluateZombieLanePolicy(state, row);
        if (lane.allowsAttack && CountZombiesInRow(state, row) == 0 && EconomyPlantsInRow(state, row) > 0) {
            ++unpressuredEconomyRows;
        }
    }
    for (const VSCardState &card : state.seedBanks[1]) {
        if (IsSlotBlocked(card.slot) || !card.active || card.refreshing || card.refreshCounter > 0) {
            continue;
        }
        const SeedType seed = static_cast<SeedType>(card.seedType);
        // Sudden death removes zombie-side economy actions.  Filter them
        // before scoring so an otherwise attractive grave cannot stall
        // the agent on a target the mode rejects.
        if (state.isSuddenDeath && IsZombieEconomySeed(seed)) {
            continue;
        }
        if (graveDefenseUrgent && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_ZOMBIE_GRAVESTONE)) {
            continue;
        }
        for (int row = 0; row < state.rows; ++row) {
            const ZombieLanePolicy lane = EvaluateZombieLanePolicy(state, row);
            const std::optional<VSGridPosition> target = FindTarget(card, row);
            if (!target.has_value() || !IsCardReadyForZombieTarget(card, state, *target)) {
                continue;
            }
            // The mower sweep makes every zombie-side action in this row
            // disposable. Do not spend a body, Bungee, grave, or screen
            // while it is moving, nor after an invader reaches column 0.
            if (lane.deploymentBlocked) {
                continue;
            }
            const int effectiveCost = static_cast<SeedType>(card.seedType) == SeedType::SEED_ZOMBIE_MOUND
                ? MoundUpgradeCostAt(state, *target)
                : card.cost;
            const bool isEconomyAction = IsZombieEconomySeed(seed);
            const bool isTargetedAction = IsZombieTargetedSeed(seed);
            const bool isProtectedGuard = IsZombieGraveGuardSeed(seed) && graveDefenseUrgent;
            const int zombiesInRow = CountZombiesInRow(state, row);
            const bool pursueBrokenMowerRow = lane.conversionRoute;
            // A destroyed zombie target cannot be recovered by spending more
            // bodies in its row. The normal marker-less VS boards retain all
            // rows through HasLiveZombieTargetInRow's compatibility path.
            if (!isEconomyAction && !lane.hasLiveTarget) {
                continue;
            }
            if (lane.strongMowerlessPlantLane && !lane.conversionRoute
                && !isEconomyAction && !isTargetedAction && !isProtectedGuard) {
                continue;
            }

            const bool isLaneAttack = !isEconomyAction && !isTargetedAction && !isProtectedGuard;
            // When the plant's broad answer is ready, a third ordinary body
            // in one row is precisely the stack it is waiting to erase.
            // Spread to a live, unpressured economy row first; once those
            // routes are all occupied, the existing score penalties still
            // permit a deliberate late-game commitment.
            if (plantHasReadyAsh && isLaneAttack && !IsHeavyZombieSeed(seed)
                && zombiesInRow >= 2 && unpressuredEconomyRows > 0 && !pursueBrokenMowerRow) {
                continue;
            }

            int score = ZombieAIPlanning::CardScore(card, state, context, row, effectiveCost);
            if (seed == SeedType::SEED_ZOMBIE_MOUND) {
                // The target already passed the per-mound affordability
                // check. Add its marginal income return so level 0/2
                // upgrades beat an expensive level 1/3 tunnel vision.
                score += MoundUpgradePriorityAt(state, *target);
            }
            const bool isEarlyHeavyCandidate =
                ZombieAIPlanning::IsEarlyHeavyCommitCard(state, seed, context);
            if ((forceOpeningPressure && !IsZombieFrontlineProbeSeed(seed) && !isEarlyHeavyCandidate)
                || (hasReadyEarlyHeavyCommit && !forceOpeningPressure && !isEarlyHeavyCandidate && isEconomyAction)
                || (preservePressureDuringRepair && !forceOpeningPressure && isEconomyAction)) {
                continue;
            }
            if (isLaneAttack && !IsHeavyZombieSeed(seed)) {
                // A row remains on cooldown for a few decisions after a
                // probe. This prevents alternating two lanes forever
                // while another Sunflower route remains untouched. A spent
                // mower is different: that live target lane is a conversion
                // route, so only a small cooldown applies.
                score -= static_cast<int>(mLaneAttackCooldown[static_cast<std::size_t>(row)])
                    * (pursueBrokenMowerRow ? 35 : 155);
            }
            if (plantHasReadyAsh && zombiesInRow > 0 && !isEconomyAction && !isTargetedAction && !isProtectedGuard) {
                // One ready Cherry/Squash/Jalapeno/Doomshroom is a reason
                // to fan out, not to build a one-row pile. Heavy cards can
                // still be a deliberate finisher, but are penalized much
                // harder once two bodies already share the blast cell.
                score -= IsHeavyZombieSeed(seed)
                    ? (zombiesInRow >= 2 ? 720 : 260)
                    : (zombiesInRow >= 2 ? 900 : 520);
            }
            if (!isEconomyAction && !isTargetedAction && !isProtectedGuard && !IsHeavyZombieSeed(seed)
                && activePressureRows < desiredOpeningRows) {
                // The new recordings use cheap cones, imps and normal
                // zombies to establish several live probes before any
                // lane receives a second body. This also denies one Ash
                // counter an entire zombie-side wave.
                score += zombiesInRow == 0 ? 210 : -280;
            }
            if (graveDefenseUrgent && row == graveDefenseRow) {
                score += isProtectedGuard ? 410 : 120;
            }
            if (preserveSurvivingFront && row == survivingFrontRow) {
                const SeedType seed = static_cast<SeedType>(card.seedType);
                if (IsZombieGraveGuardSeed(seed) && !survivingFrontGuarded) {
                    // After two attack lanes have been cleared, keep the
                    // remaining valuable front alive before restarting
                    // economic expansion on an empty route.
                    score += criticalTargetRow >= 0 ? 560 : 320;
                } else if (!IsHeavyZombieSeed(seed) && !IsZombieEconomySeed(seed)) {
                    score += criticalTargetRow >= 0 ? 110 : 75;
                }
            }
            if (saveForHeavy && !IsHeavyZombieSeed(static_cast<SeedType>(card.seedType))) {
                // Continue inexpensive probes and grave guards, but do
                // not repeatedly spend the giant timing on medium cards.
                // This keeps 100/200-brain finishers reachable without
                // leaving every grave route unprotected.
                if (!isProtectedGuard) {
                    const int lowCostPenalty = tempo.IsEnhanced() ? -120 : -45;
                    const int mediumCostPenalty = tempo.IsEnhanced() ? -330 : -190;
                    score += card.cost <= std::max(50, heavyZombieReserve / 4) ? lowCostPenalty : mediumCostPenalty;
                }
            }
            if (isLaneAttack && !IsHeavyZombieSeed(seed) && zombiesInRow > 0 && unpressuredEconomyRows > 0
                && !pursueBrokenMowerRow) {
                score -= (zombiesInRow == 1 ? 250 : 460) * std::min(2, unpressuredEconomyRows);
            }
            if (!graveDefenseUrgent && !preserveSurvivingFront && row == mLastAttackRow && !pursueBrokenMowerRow) {
                // Do not keep feeding the same lane while another lane can
                // accept a zombie. This penalty is intentionally skipped
                // during urgent grave defense.
                score -= tempo.HasAttackCommitPressure(activePressureRows, 2, state.rows) ? 210 : 125;
            }
            if (pursueBrokenMowerRow && !IsZombieEconomySeed(seed) && !IsZombieTargetedSeed(seed)) {
                // A cleared mower lane is a live conversion route. Keep
                // pressure there while the independent grave-defense path
                // continues to protect the zombie economy. This must also
                // overcome the ordinary multi-lane spreading bias: that
                // bias is correct before a mower falls, but not when the
                // next successful push wins through this still-live target.
                score += MowerlessLaneCommitmentBonus(lane, zombiesInRow);
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
    if (IsZombieFrontlineProbeSeed(chosenSeed)) {
        mLastPressureEconomyCount = std::max(mLastPressureEconomyCount, actualEconomyCount);
    }
    if (!IsZombieEconomySeed(chosenSeed) && !IsZombieTargetedSeed(chosenSeed)) {
        mLastAttackRow = targetRow;
        if (targetRow >= 0 && targetRow < static_cast<int>(mLaneAttackCooldown.size())
                    && !(IsZombieGraveGuardSeed(chosenSeed) && graveDefenseUrgent)) {
            mLaneAttackCooldown[static_cast<std::size_t>(targetRow)] = tempo.LaneAttackCooldown(chosenSeed);
        }
    }
    return MakePlayAction(VSSide::Zombies, *bestCard, *target, state.boardTick);
}

} // namespace vsai::detail
