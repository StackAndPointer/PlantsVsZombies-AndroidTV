#include "ZombieAI.h"

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

    const int economyCount = CountZombieEconomy(state);
    if (mLastPressureEconomyCount > economyCount) {
        // A destroyed grave re-opens the pressure cadence. Rebuilding
        // from a smaller base should not force a full 15-grave rebuild
        // before the next low-cost probe is allowed.
        mLastPressureEconomyCount = economyCount - 1;
    }
    const int economyTarget = state.isSuddenDeath ? economyCount : state.rows * 3;
    const int economyDeficit = std::max(0, economyTarget - economyCount);
    const int graveDefenseRow = MostThreatenedEconomyRow(state);
    const int graveDefenseScore = ProtectableGraveThreatScore(state, graveDefenseRow);
    const int graveStraightThreat = StraightProjectileThreatScore(state, graveDefenseRow);
    const int graveLobbedThreat = LobbedProjectileThreatScore(state, graveDefenseRow);
    const int graveScreenDeficit = ZombieGraveScreenDeficit(state, graveDefenseRow);
    const bool hasGraveGuard = HasZombieGraveGuardInRow(state, graveDefenseRow);
    const bool proactiveGraveScreen = NeedsProactiveGraveScreen(state, graveDefenseRow);
    const bool graveDefenseUrgent = graveDefenseScore >= 50 || graveStraightThreat >= 55
        || graveLobbedThreat >= 70 || graveScreenDeficit >= 55 || proactiveGraveScreen;
    const bool graveDefenseReinforcement = graveDefenseUrgent && (!hasGraveGuard || graveScreenDeficit >= 55);
    if (graveLobbedThreat >= 70 && graveStraightThreat < 70) {
        if (std::optional<VSAction> action = ZombieAIPlanning::TryCounterLobbedGravePressure(state, graveDefenseRow)) {
            return action;
        }
    }
    if (graveDefenseReinforcement) {
        if (std::optional<VSAction> action = ZombieAIPlanning::TryProtectEconomy(state, graveDefenseRow)) {
            return action;
        }
    }
    const int activePressureRows = CountActiveZombieRows(state);
    const int heavyZombieReserve = ZombieAIPlanning::HeavyZombieReserve(state);
    const int heavyEconomyThreshold = HeavyZombieEconomyThreshold(state);
    // High-cost cards are a deliberate conversion of a developed grave
    // economy, not an opening all-in.  Start banking before the final two
    // graves only when multiple routes already tax the plant player.
    const bool bankForHeavy = heavyZombieReserve >= 100
        && economyCount >= std::max(state.rows + 2, heavyEconomyThreshold - 2)
        && activePressureRows >= 2 && CountLivePlants(state) >= state.rows && graveDefenseScore < 100;
    const int minimumOpeningEconomy = std::min(2, std::max(1, state.rows));
    const int desiredOpeningRows = std::min(3, state.rows);
    const bool hasReadyFrontlineProbe = ZombieAIPlanning::HasReadyFrontlineProbe(state);
    const bool hasReadyEarlyHeavyCommit = ZombieAIPlanning::HasReadyEarlyHeavyCommit(state, economyCount, activePressureRows);
    const auto HasZombieCard = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[1].begin(), state.seedBanks[1].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };
    // The Normal/Trashcan/Dog/Football/Giant replay opens with a single
    // Normal immediately after its first grave. That cheap probe forces a
    // response while the later Trashcan still has an economy worth guarding;
    // it is not the same as a generic all-in after one grave.
    const bool armoredNormalRushTemplate = HasZombieCard(SeedType::SEED_ZOMBIE_NORMAL)
        && HasZombieCard(SeedType::SEED_ZOMBIE_TRASHCAN) && HasZombieCard(SeedType::SEED_ZOMBIE_DOGWALKER)
        && HasZombieCard(SeedType::SEED_ZOMBIE_FOOTBALL)
        && (HasZombieCard(SeedType::SEED_ZOMBIE_GARGANTUAR) || HasZombieCard(SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR));
    const bool impPailSundayTemplate = HasZombieCard(SeedType::SEED_ZOMBIE_IMP)
        && HasZombieCard(SeedType::SEED_ZOMBIE_PAIL) && HasZombieCard(SeedType::SEED_ZOMBIE_SUNDAY_EDITION)
        && HasZombieCard(SeedType::SEED_ZOMBIE_SCREEN_DOOR);
    const bool zamboniPoleOpeningTemplate = HasZombieCard(SeedType::SEED_ZOMBONI)
        && HasZombieCard(SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER) && HasZombieCard(SeedType::SEED_ZOMBIE_PAIL)
        && HasZombieCard(SeedType::SEED_ZOMBIE_TRAFFIC_CONE) && HasZombieCard(SeedType::SEED_ZOMBIE_IMP);
    // Both recorded lines establish three rear graves before their first
    // Imp/Zomboni probe. Two graves give neither the later Sunday release
    // nor a returned Zomboni enough economy to stay on the board.
    const int openingPressureEconomyFloor = (impPailSundayTemplate || zamboniPoleOpeningTemplate)
        ? std::min(3, state.rows)
        : minimumOpeningEconomy;
    const bool firstGraveProbe = armoredNormalRushTemplate && economyCount == 1 && activePressureRows == 0
        && hasReadyFrontlineProbe && mLastPressureEconomyCount < economyCount;
    // Winning zombie replays establish a few rear graves, then alternate
    // a probe with another grave. One uninterrupted build to 15 gives the
    // plant side a free economic opening and never creates a threat lane.
    const bool forceOpeningPressure = firstGraveProbe || (economyCount >= openingPressureEconomyFloor && economyCount <= state.rows + 1
        && activePressureRows < desiredOpeningRows && hasReadyFrontlineProbe && mLastPressureEconomyCount < economyCount);
    const bool preservePressureDuringRepair = economyCount >= minimumOpeningEconomy && economyDeficit <= 2
        && activePressureRows > 0 && hasReadyFrontlineProbe;
    const int survivingFrontRow = MostValuableZombieFrontRow(state);
    const int survivingFrontValue = ZombieFrontlineValueInRow(state, survivingFrontRow);
    const bool preserveSurvivingFront = economyCount >= state.rows && activePressureRows == 1 && survivingFrontValue >= 90;
    const bool survivingFrontGuarded = HasZombieGraveGuardInRow(state, survivingFrontRow);
    const int economicRow = economyCount < state.rows * 2 ? ZombieAIPlanning::LeastCommittedZombieRow(state) : LeastThreatenedEconomyRow(state);
    const bool restorationCanProceed = !graveDefenseReinforcement || hasGraveGuard;
    const bool restorationOutweighsFront = economyDeficit >= 2 || graveDefenseScore < 100 || hasGraveGuard;
    const bool economyRepairIsUrgent = economyCount < minimumOpeningEconomy || economyDeficit >= 3;
    if (economyDeficit > 0 && restorationCanProceed && !forceOpeningPressure && !hasReadyEarlyHeavyCommit && !bankForHeavy
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
        if (hasLobbedPlant && (seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR || seed == SeedType::SEED_ZOMBIE_NEWSPAPER
                               || seed == SeedType::SEED_ZOMBIE_TRASHCAN)) {
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
        if (IsTargetedSeed(card.seedType)) {
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
        if (!IsMowerInMotion(state, row) && !IsMowerAboutToTrigger(state, row) && !IsMowerlessStrongPlantLane(state, row)
            && CountZombiesInRow(state, row) == 0 && EconomyPlantsInRow(state, row) > 0) {
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
        if (state.isSuddenDeath && (seed == SeedType::SEED_ZOMBIE_GRAVESTONE || seed == SeedType::SEED_ZOMBIE_MOUND)) {
            continue;
        }
        if (graveDefenseUrgent && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_ZOMBIE_GRAVESTONE)) {
            continue;
        }
        for (int row = 0; row < state.rows; ++row) {
            const std::optional<VSGridPosition> target = FindTarget(card, row);
            if (!target.has_value() || !IsCardReadyForZombieTarget(card, state, *target)) {
                continue;
            }
            // The mower sweep makes every zombie-side action in this row
            // disposable. Do not spend a body, Bungee, grave, or screen
            // while it is moving, nor after an invader reaches column 0.
            if (IsMowerInMotion(state, row) || IsMowerAboutToTrigger(state, row)) {
                continue;
            }
            const int effectiveCost = static_cast<SeedType>(card.seedType) == SeedType::SEED_ZOMBIE_MOUND
                ? MoundUpgradeCostAt(state, *target)
                : card.cost;
            const bool isEconomyAction = ZombieAIPlanning::IsEconomySeed(seed);
            const bool isTargetedAction = ZombieAIPlanning::IsTargetedSeed(card.seedType);
            const bool isProtectedGuard = IsZombieGraveGuardSeed(seed) && graveDefenseUrgent;
            const int zombiesInRow = CountZombiesInRow(state, row);
            const bool mowerGone = row < static_cast<int>(state.mowerAvailable.size())
                && !state.mowerAvailable[static_cast<std::size_t>(row)] && !IsMowerInMotion(state, row);
            // A mowerless lane with working firepower has already paid for
            // its plant-side conversion. Avoid re-opening it with fresh
            // bodies or a grave screen; attack an intact route instead.
            if (mowerGone && zombiesInRow == 0 && !isTargetedAction) {
                continue;
            }
            if (IsMowerlessStrongPlantLane(state, row) && !(mowerGone && zombiesInRow > 0)
                && !isEconomyAction && !isTargetedAction && !isProtectedGuard) {
                continue;
            }

            int score = ZombieAIPlanning::CardScore(card, state, row, economyCount, effectiveCost);
            if (seed == SeedType::SEED_ZOMBIE_MOUND) {
                // The target already passed the per-mound affordability
                // check. Add its marginal income return so level 0/2
                // upgrades beat an expensive level 1/3 tunnel vision.
                score += MoundUpgradePriorityAt(state, *target);
            }
            const bool isEarlyHeavyCandidate =
                ZombieAIPlanning::IsEarlyHeavyCommitCard(state, seed, economyCount, activePressureRows);
            if ((forceOpeningPressure && !IsFrontlineProbeSeed(seed) && !isEarlyHeavyCandidate)
                || (hasReadyEarlyHeavyCommit && !forceOpeningPressure && !isEarlyHeavyCandidate && isEconomyAction)
                || (preservePressureDuringRepair && !forceOpeningPressure && isEconomyAction)) {
                continue;
            }
            const bool isLaneAttack = !isEconomyAction && !isTargetedAction && !isProtectedGuard;
            if (isLaneAttack && !IsHeavyZombieSeed(seed)) {
                // A row remains on cooldown for a few decisions after a
                // probe. This prevents alternating two lanes forever
                // while another Sunflower route remains untouched.
                score -= static_cast<int>(mLaneAttackCooldown[static_cast<std::size_t>(row)]) * 155;
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
                    score += 320;
                } else if (!IsHeavyZombieSeed(seed) && seed != SeedType::SEED_ZOMBIE_GRAVESTONE
                           && seed != SeedType::SEED_ZOMBIE_MOUND) {
                    score += 75;
                }
            }
            if (saveForHeavy && !IsHeavyZombieSeed(static_cast<SeedType>(card.seedType))) {
                // Continue inexpensive probes and grave guards, but do
                // not repeatedly spend the giant timing on medium cards.
                // This keeps 100/200-brain finishers reachable without
                // leaving every grave route unprotected.
                score += card.cost <= std::max(50, heavyZombieReserve / 4) ? -45 : -190;
            }
            const bool pursueBrokenMowerRow = mowerGone
                && CountPlantsInRow(state, row) > 0 && zombiesInRow > 0;
            if (isLaneAttack && !IsHeavyZombieSeed(seed) && zombiesInRow > 0 && unpressuredEconomyRows > 0
                && !pursueBrokenMowerRow) {
                score -= (zombiesInRow == 1 ? 250 : 460) * std::min(2, unpressuredEconomyRows);
            }
            if (!graveDefenseUrgent && !preserveSurvivingFront && row == mLastAttackRow && !pursueBrokenMowerRow) {
                // Do not keep feeding the same lane while another lane can
                // accept a zombie. This penalty is intentionally skipped
                // during urgent grave defense.
                score -= activePressureRows >= 2 ? 210 : 125;
            }
            if (pursueBrokenMowerRow && !IsEconomySeed(seed) && !IsTargetedSeed(card.seedType)) {
                // A cleared mower lane is a live conversion route. Keep
                // pressure there while the independent grave-defense path
                // continues to protect the zombie economy.
                score += 420;
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
    if (IsFrontlineProbeSeed(chosenSeed)) {
        mLastPressureEconomyCount = std::max(mLastPressureEconomyCount, economyCount);
    }
    if (chosenSeed != SeedType::SEED_ZOMBIE_GRAVESTONE && chosenSeed != SeedType::SEED_ZOMBIE_MOUND
        && chosenSeed != SeedType::SEED_ZOMBIE_BUNGEE) {
        mLastAttackRow = targetRow;
        if (targetRow >= 0 && targetRow < static_cast<int>(mLaneAttackCooldown.size())
            && !(IsZombieGraveGuardSeed(chosenSeed) && graveDefenseUrgent)) {
            mLaneAttackCooldown[static_cast<std::size_t>(targetRow)] = ZombieAIPlanning::IsFastAttackSeed(chosenSeed) ? 4 : 3;
        }
    }
    return MakePlayAction(VSSide::Zombies, *bestCard, *target, state.boardTick);
}

} // namespace vsai::detail
