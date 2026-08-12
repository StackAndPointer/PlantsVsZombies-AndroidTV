#include "ZombieAI.h"

#include "../VSActionAILanePolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace vsai::detail {

bool ZombieAIPlanning::HasLobbedPlantInRow(const VSGameState &state, int row) {
    return std::any_of(state.plants.begin(), state.plants.end(), [row](const VSPlantState &plant) {
        if (IsDeadOrOutside(plant) || plant.position.row != row) {
            return false;
        }
        switch (static_cast<SeedType>(plant.seedType)) {
            case SeedType::SEED_CABBAGEPULT:
            case SeedType::SEED_KERNELPULT:
            case SeedType::SEED_MELONPULT:
            case SeedType::SEED_WINTERMELON:
            case SeedType::SEED_COBCANNON:
            case SeedType::SEED_SPORESHROOM:
                return true;
            default:
                return false;
        }
    });
}

const VSCardState *ZombieAIPlanning::FindReadyCard(const VSGameState &state, SeedType seedType) const {
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

int ZombieAIPlanning::HeavyZombieReserve(const VSGameState &state) const {
    int reserve = std::numeric_limits<int>::max();
    for (const VSCardState &card : state.seedBanks[1]) {
        if (IsSlotBlocked(card.slot) || card.matchRestricted || !card.active || card.seedType == static_cast<std::uint16_t>(SeedType::SEED_NONE)) {
            continue;
        }
        if (IsHeavyZombieSeed(static_cast<SeedType>(card.seedType))) {
            reserve = std::min(reserve, std::max(0, card.cost));
        }
    }
    return reserve == std::numeric_limits<int>::max() ? 0 : reserve;
}

bool ZombieAIPlanning::HasReadyFrontlineProbe(const VSGameState &state) const {
    return std::any_of(state.seedBanks[1].begin(), state.seedBanks[1].end(), [&](const VSCardState &card) {
        return !IsSlotBlocked(card.slot) && card.active && !card.matchRestricted && IsZombieFrontlineProbeSeed(static_cast<SeedType>(card.seedType))
            && IsReadyCard(card, state.zombieBrains);
    });
}

bool ZombieAIPlanning::IsEarlyHeavyCommitCard(const VSGameState &state, SeedType seed, int economyCount, int activePressureRows) const {
    if (!IsHeavyZombieSeed(seed)) {
        return false;
    }
    const bool replayPoleTemplate = HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_NEWSPAPER)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_TRAFFIC_CONE) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_LADDER)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_BOBSLED) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER);
    const bool replayFanPoleTemplate = HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_NORMAL)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_NEWSPAPER) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_SUPER_FAN_IMP)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_GIGA_FOOTBALL) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_DOGWALKER);
    const bool replayFlagGigaTemplate = HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_FLAG)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_SQUASH_HEAD) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_SCREEN_DOOR)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_TRAFFIC_CONE) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR);
    const bool replayArmoredNormalTemplate = HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_NORMAL)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_TRASHCAN) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_DOGWALKER)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_FOOTBALL) && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR);
    const int livePlants = CountLivePlants(state);
    // These recordings have exceptional, but not arbitrary, early
    // conversions. They still need a real plant board and either a
    // live probe or multiple spread probes before the heavy card may
    // interrupt the normal grave-building cadence.
    if (seed == SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER && replayPoleTemplate
        && economyCount >= 2 && activePressureRows >= 1 && livePlants >= 3) {
        return true;
    }
    if (seed == SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER && replayFanPoleTemplate
        && economyCount >= 3 && activePressureRows >= 2 && livePlants >= state.rows) {
        return true;
    }
    if (seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR && replayFlagGigaTemplate
        && economyCount >= 8 && activePressureRows >= 2 && livePlants >= state.rows) {
        return true;
    }
    // The Normal/Trashcan/Dog replay banks behind protected graves until
    // roughly eight income sources, then turns its broad cheap pressure
    // into the first Giga Gargantuar. This is earlier than the generic
    // finisher threshold, but still needs two live routes and a real board.
    if (seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR && replayArmoredNormalTemplate
        && economyCount >= 8 && activePressureRows >= 2 && livePlants >= state.rows) {
        return true;
    }
    if (activePressureRows < 2 || livePlants < state.rows) {
        return false;
    }
    const int minimumEconomy = seed == SeedType::SEED_ZOMBIE_GARGANTUAR ? state.rows : std::max(state.rows * 2, state.rows + 3);
    return economyCount >= minimumEconomy;
}

bool ZombieAIPlanning::HasReadyEarlyHeavyCommit(const VSGameState &state, int economyCount, int activePressureRows) const {
    return std::any_of(state.seedBanks[1].begin(), state.seedBanks[1].end(), [&](const VSCardState &card) {
        const SeedType seed = static_cast<SeedType>(card.seedType);
        return !IsSlotBlocked(card.slot) && card.active && !card.matchRestricted && IsReadyCard(card, state.zombieBrains)
            && IsEarlyHeavyCommitCard(state, seed, economyCount, activePressureRows);
    });
}

std::optional<VSAction> ZombieAIPlanning::TryTemplateSundayRelease(const VSGameState &state, int economyCount, int activePressureRows) {
    if (activePressureRows < 2) {
        return std::nullopt;
    }

    const bool normalNewsImpSundayTemplate = HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_NORMAL)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_DOGWALKER)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_NEWSPAPER)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_IMP)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_SUNDAY_EDITION);
    const bool impPailSledSundayTemplate = HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_IMP)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_PAIL)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_BOBSLED)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_SUNDAY_EDITION)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_SCREEN_DOOR);
    const bool peaHeadSundayTemplate = HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_PEA_HEAD)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_IMP)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_TRASHCAN)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_SUNDAY_EDITION)
        && HasActiveDeckCard(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_GARGANTUAR);
    const int peaHeadCount = static_cast<int>(std::count_if(state.zombies.begin(), state.zombies.end(), [](const VSZombieState &zombie) {
        return !zombie.dead && zombie.zombieType == static_cast<std::uint16_t>(ZombieType::ZOMBIE_PEA_HEAD);
    }));
    const int maximumCounterExposure = impPailSledSundayTemplate || peaHeadSundayTemplate ? 145 : 150;
    const bool releaseWindow = (normalNewsImpSundayTemplate && economyCount >= std::max(3, state.rows - 1))
        || (impPailSledSundayTemplate && economyCount >= std::max(state.rows + 3, 8))
        || (peaHeadSundayTemplate && economyCount >= state.rows + 2 && peaHeadCount >= 2);
    const VSCardState *sundayEdition = FindReadyCard(state, SeedType::SEED_ZOMBIE_SUNDAY_EDITION);
    if (!releaseWindow || sundayEdition == nullptr) {
        return std::nullopt;
    }

    int bestRow = -1;
    int bestScore = std::numeric_limits<int>::min();
    for (int row = 0; row < state.rows; ++row) {
        const VSGridPosition target = FindZombieCell(state, SeedType::SEED_ZOMBIE_SUNDAY_EDITION, row);
        if (EvaluateZombieLanePolicy(state, row).allowsAttack && target.col >= 0 && target.row >= 0
            && IsCardReadyForZombieTarget(*sundayEdition, state, target)
            && PlantAreaCounterExposure(state, row) < maximumCounterExposure) {
            const int score = ZombieLaneAttackScore(state, row) + PlantEconomyValueInRow(state, row)
                + SustainedOutputScoreInRow(state, row) - PlantAreaCounterExposure(state, row);
            if (bestRow < 0 || score > bestScore) {
                bestRow = row;
                bestScore = score;
            }
        }
    }
    if (bestRow < 0) {
        return std::nullopt;
    }
    return MakePlayAction(VSSide::Zombies, *sundayEdition,
        FindZombieCell(state, SeedType::SEED_ZOMBIE_SUNDAY_EDITION, bestRow), state.boardTick);
}

std::unique_ptr<IVSAgent> CreateZombieAI() {
    return std::make_unique<ZombieAI>();
}

} // namespace vsai::detail
