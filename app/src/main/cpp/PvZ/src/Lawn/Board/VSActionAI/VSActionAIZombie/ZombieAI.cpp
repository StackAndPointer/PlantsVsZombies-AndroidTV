#include "ZombieAI.h"

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

bool ZombieAIPlanning::IsTargetedSeed(std::uint16_t seed) {
    const SeedType seedType = static_cast<SeedType>(seed);
    return seedType == SeedType::SEED_ZOMBIE_BUNGEE;
}

bool ZombieAIPlanning::IsEconomySeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_GRAVESTONE || seed == SeedType::SEED_ZOMBIE_MOUND;
}

bool ZombieAIPlanning::IsFrontlineProbeSeed(SeedType seed) {
    if (IsEconomySeed(seed) || ZombieAIPlanning::IsTargetedSeed(static_cast<std::uint16_t>(seed)) || IsHeavyZombieSeed(seed)) {
        return false;
    }
    // Trashcan and the nut heads are dedicated grave screens. The other
    // cheap guard cards still make useful opening probes in the replays.
    return seed != SeedType::SEED_ZOMBIE_TRASHCAN && seed != SeedType::SEED_ZOMBIE_WALLNUT_HEAD
        && seed != SeedType::SEED_ZOMBIE_TALLNUT_HEAD;
}

bool ZombieAIPlanning::IsFastAttackSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_NORMAL:
        case SeedType::SEED_ZOMBIE_IMP:
        case SeedType::SEED_ZOMBIE_SUPER_FAN_IMP:
        case SeedType::SEED_ZOMBIE_DOGWALKER:
        case SeedType::SEED_ZOMBIE_FLAG:
        case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
            return true;
        default:
            return false;
    }
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
        return !IsSlotBlocked(card.slot) && card.active && !card.matchRestricted && ZombieAIPlanning::IsFrontlineProbeSeed(static_cast<SeedType>(card.seedType))
            && IsReadyCard(card, state.zombieBrains);
    });
}

bool ZombieAIPlanning::IsEarlyHeavyCommitCard(const VSGameState &state, SeedType seed, int economyCount, int activePressureRows) const {
    if (!IsHeavyZombieSeed(seed)) {
        return false;
    }
    const auto HasZombieCard = [&state](SeedType seed) {
        return std::any_of(state.seedBanks[1].begin(), state.seedBanks[1].end(), [seed](const VSCardState &card) {
            return card.active && !card.matchRestricted && card.seedType == static_cast<std::uint16_t>(seed);
        });
    };
    const bool replayPoleTemplate = HasZombieCard(SeedType::SEED_ZOMBIE_NEWSPAPER)
        && HasZombieCard(SeedType::SEED_ZOMBIE_TRAFFIC_CONE) && HasZombieCard(SeedType::SEED_ZOMBIE_LADDER)
        && HasZombieCard(SeedType::SEED_ZOMBIE_BOBSLED) && HasZombieCard(SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER);
    const bool replayFanPoleTemplate = HasZombieCard(SeedType::SEED_ZOMBIE_NORMAL)
        && HasZombieCard(SeedType::SEED_ZOMBIE_NEWSPAPER) && HasZombieCard(SeedType::SEED_ZOMBIE_SUPER_FAN_IMP)
        && HasZombieCard(SeedType::SEED_ZOMBIE_GIGA_FOOTBALL) && HasZombieCard(SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER)
        && HasZombieCard(SeedType::SEED_ZOMBIE_DOGWALKER);
    const bool replayFlagGigaTemplate = HasZombieCard(SeedType::SEED_ZOMBIE_FLAG)
        && HasZombieCard(SeedType::SEED_ZOMBIE_SQUASH_HEAD) && HasZombieCard(SeedType::SEED_ZOMBIE_SCREEN_DOOR)
        && HasZombieCard(SeedType::SEED_ZOMBIE_TRAFFIC_CONE) && HasZombieCard(SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR);
    const bool replayArmoredNormalTemplate = HasZombieCard(SeedType::SEED_ZOMBIE_NORMAL)
        && HasZombieCard(SeedType::SEED_ZOMBIE_TRASHCAN) && HasZombieCard(SeedType::SEED_ZOMBIE_DOGWALKER)
        && HasZombieCard(SeedType::SEED_ZOMBIE_FOOTBALL) && HasZombieCard(SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR);
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

std::unique_ptr<IVSAgent> CreateZombieAI() {
    return std::make_unique<ZombieAI>();
}

} // namespace vsai::detail
