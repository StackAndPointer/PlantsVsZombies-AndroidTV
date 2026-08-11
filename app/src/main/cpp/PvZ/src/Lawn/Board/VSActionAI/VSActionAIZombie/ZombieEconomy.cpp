#include "ZombieAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace vsai::detail {

std::optional<VSAction> ZombieAIPlanning::TryBuildEconomy(const VSGameState &state, int row) {
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

int ZombieAIPlanning::GraveGuardPriority(SeedType seed) {
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

std::optional<VSAction> ZombieAIPlanning::TryProtectEconomy(const VSGameState &state, int row) {
    if (IsMowerInMotion(state, row) || IsMowerAboutToTrigger(state, row) || IsMowerlessStrongPlantLane(state, row)) {
        return std::nullopt;
    }
    const int protectableThreat = ProtectableGraveThreatScore(state, row);
    const int straightThreat = StraightProjectileThreatScore(state, row);
    const int lobbedThreat = LobbedProjectileThreatScore(state, row);
    const int screenDeficit = ZombieGraveScreenDeficit(state, row);
    const bool proactiveScreen = NeedsProactiveGraveScreen(state, row);
    if (row < 0 || row >= state.rows || (HasZombieGraveGuardInRow(state, row) && screenDeficit < 80)
        || (!proactiveScreen && protectableThreat < 80 && straightThreat < 80 && lobbedThreat < 95 && screenDeficit < 80)) {
        return std::nullopt;
    }

    const VSCardState *bestCard = nullptr;
    int bestScore = std::numeric_limits<int>::min();
    const bool plantHasMagnet = std::any_of(state.seedBanks[0].begin(), state.seedBanks[0].end(), [](const VSCardState &card) {
        return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(SeedType::SEED_MAGNETSHROOM);
    }) || CountPlantType(state, SeedType::SEED_MAGNETSHROOM) > 0;
    const auto IsMetalGuard = [](SeedType seed) {
        return seed == SeedType::SEED_ZOMBIE_PAIL || seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR
            || seed == SeedType::SEED_ZOMBIE_TRASHCAN;
    };
    for (const VSCardState &card : state.seedBanks[1]) {
        const SeedType seed = static_cast<SeedType>(card.seedType);
        if (IsSlotBlocked(card.slot) || !IsZombieGraveGuardSeed(seed) || !IsReadyCard(card, state.zombieBrains)) {
            continue;
        }
        // A pult, including Spore-shroom, attacks over a slow screen.
        // Do not turn an urgent grave-defense branch into a free
        // Trashcan, Door, or Newspaper donation.
        if (HasLobbedPlantInRow(state, row)
            && (seed == SeedType::SEED_ZOMBIE_TRASHCAN || seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR
                || seed == SeedType::SEED_ZOMBIE_NEWSPAPER)) {
            continue;
        }
        const VSGridPosition target = FindZombieCell(state, seed, row);
        if (!IsCardReadyForZombieTarget(card, state, target)) {
            continue;
        }

        int score = ZombieAIPlanning::GraveGuardPriority(seed) + protectableThreat * 2 + (proactiveScreen ? 220 : 0);
        // Trashcan is the direct-fire shield from the recordings. Lobbed
        // projectiles pass over every metal screen, so the row filter
        // above leaves durable non-screen heads as the only protection
        // candidates in those lanes.
        if (seed == SeedType::SEED_ZOMBIE_TRASHCAN) {
            score += straightThreat * 3 - lobbedThreat * 2;
        } else {
            score += straightThreat > 0 ? 120 : 0;
            score += lobbedThreat > 0 ? 170 : 0;
        }
        if (plantHasMagnet) {
            score += IsMetalGuard(seed) ? -420 : 260;
        }
        score += screenDeficit * 2;
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

std::optional<VSAction> ZombieAIPlanning::TryCounterLobbedGravePressure(const VSGameState &state, int row) {
    const VSCardState *catapult = FindReadyCard(state, SeedType::SEED_ZOMBIE_CATAPULT);
    if (catapult == nullptr || CountZombieEconomy(state) < state.rows || HasMindControlledZombieInRow(state, row)
        || LobbedProjectileThreatScore(state, row) < 95) {
        return std::nullopt;
    }

    // A Catapult is the replay-derived answer to a developed pult or
    // Spore firing lane. Unlike a Door, Newspaper, or Trashcan, it does
    // not donate a slow metal screen to a projectile that arcs over it.
    const VSGridPosition target = FindZombieCell(state, SeedType::SEED_ZOMBIE_CATAPULT, row);
    if (target.col < 0 || target.row < 0 || !IsCardReadyForZombieTarget(*catapult, state, target)) {
        return std::nullopt;
    }
    return MakePlayAction(VSSide::Zombies, *catapult, target, state.boardTick);
}

} // namespace vsai::detail
