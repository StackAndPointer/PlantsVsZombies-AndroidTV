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

#include "VSActionAIStrategy.h"

#include "PvZ/SexyAppFramework/Buffer.h"
#include "PvZ/SexyAppFramework/SexyAppBase.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace vsai::detail {

struct StrategyRule {
    VSSide side = VSSide::Plants;
    std::uint16_t seed = 0;
    std::uint32_t deckSignature = 0;
    int phase = 0;
    std::array<int, 4> buckets = {-1, -1, -1, -1};
    int bonus = 0;
};

constexpr std::array<unsigned char, 8> kStrategyDatabaseMagic = {'P', 'V', 'Z', 'V', 'S', 'D', 'B', '\0'};
constexpr std::uint16_t kStrategyDatabaseVersion = 2;
constexpr std::uint16_t kLegacyStrategyDatabaseVersion = 1;
constexpr std::size_t kStrategyDatabaseHeaderSize = 12;
constexpr std::size_t kLegacyStrategyDatabaseRuleSize = 12;
constexpr std::size_t kStrategyDatabaseRuleSize = 16;

std::uint16_t ReadStrategyU16(const std::vector<unsigned char> &data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) | (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

std::uint32_t ReadStrategyU32(const std::vector<unsigned char> &data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(data[offset + 2]) << 16) | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::uint32_t DeckSignature(const VSGameState &state, VSSide side) {
    const std::size_t sideIndex = side == VSSide::Plants ? 0 : 1;
    std::vector<std::uint16_t> seeds;
    for (const VSCardState &card : state.seedBanks[sideIndex]) {
        if (!card.active || card.matchRestricted || card.seedType == static_cast<std::uint16_t>(SeedType::SEED_NONE)) {
            continue;
        }
        const SeedType seed = static_cast<SeedType>(card.seedType);
        // Economy cards are baseline slots, not the tactical identity of a
        // replay template. Some recordings omit them from metadata while the
        // local chooser includes them, so omit them from both hash builders.
        if ((side == VSSide::Plants && (seed == SeedType::SEED_SUNFLOWER || seed == SeedType::SEED_SUNSHROOM))
            || (side == VSSide::Zombies && seed == SeedType::SEED_ZOMBIE_GRAVESTONE)) {
            continue;
        }
        seeds.push_back(card.seedType);
    }
    std::sort(seeds.begin(), seeds.end());

    // FNV-1a over sorted 16-bit seed ids. Keep this byte-for-byte aligned
    // with the external replay extractor: deck order is UI noise, whereas
    // its card composition identifies the tactical template.
    std::uint32_t value = 2166136261U;
    for (const std::uint16_t seed : seeds) {
        for (const unsigned char byte : {static_cast<unsigned char>(seed & 0xFF), static_cast<unsigned char>((seed >> 8) & 0xFF)}) {
            value ^= byte;
            value *= 16777619U;
        }
    }
    value ^= static_cast<std::uint32_t>(seeds.size());
    return value * 16777619U;
}

class StrategyDatabase {
    std::vector<StrategyRule> mRules;
    bool mLoaded = false;

    void Load() {
        if (mLoaded || Sexy::gSexyAppBase == nullptr) {
            return;
        }
        mLoaded = true;

        Sexy::Buffer buffer;
        if (!Sexy::gSexyAppBase->ReadBufferFromFile("addonFiles/data/vs_ai_strategy_db.bin", &buffer, false)) {
            return;
        }

        const auto &data = buffer.mData;
        if (data.size() < kStrategyDatabaseHeaderSize || !std::equal(kStrategyDatabaseMagic.begin(), kStrategyDatabaseMagic.end(), data.begin())) {
            return;
        }
        const std::uint16_t version = ReadStrategyU16(data, 8);
        const bool legacyDatabase = version == kLegacyStrategyDatabaseVersion;
        if (!legacyDatabase && version != kStrategyDatabaseVersion) {
            return;
        }
        const std::size_t ruleCount = ReadStrategyU16(data, 10);
        const std::size_t ruleSize = legacyDatabase ? kLegacyStrategyDatabaseRuleSize : kStrategyDatabaseRuleSize;
        if (ruleCount > (data.size() - kStrategyDatabaseHeaderSize) / ruleSize
            || kStrategyDatabaseHeaderSize + ruleCount * ruleSize != data.size()) {
            return;
        }

        for (std::size_t index = 0; index < ruleCount; ++index) {
            const std::size_t offset = kStrategyDatabaseHeaderSize + index * ruleSize;
            const unsigned char sideCode = data[offset];
            const int phase = data[offset + (legacyDatabase ? 3 : 7)];
            const int bonus = data[offset + (legacyDatabase ? 8 : 12)];
            if (sideCode > 1 || phase < 0 || phase > 2 || bonus <= 0 || bonus > 100) {
                continue;
            }

            StrategyRule rule{};
            rule.side = sideCode == 0 ? VSSide::Plants : VSSide::Zombies;
            rule.seed = ReadStrategyU16(data, offset + 1);
            rule.deckSignature = legacyDatabase ? 0 : ReadStrategyU32(data, offset + 3);
            rule.phase = phase;
            bool validRule = true;
            for (std::size_t bucketIndex = 0; bucketIndex < rule.buckets.size(); ++bucketIndex) {
                const std::size_t bucketOffset = legacyDatabase ? 4 : 8;
                const int bucket = static_cast<int>(static_cast<std::int8_t>(data[offset + bucketOffset + bucketIndex]));
                if (bucket < -1 || bucket > 3) {
                    validRule = false;
                    break;
                }
                rule.buckets[bucketIndex] = bucket;
            }
            if (!validRule) {
                continue;
            }
            rule.bonus = bonus;
            mRules.push_back(rule);
        }
    }

public:
    int Bonus(const VSGameState &state, VSSide side, SeedType seed, int targetRow) {
        Load();
        if (mRules.empty() || targetRow < 0 || targetRow >= state.rows) {
            return 0;
        }

        const int ownEconomy = side == VSSide::Plants ? CountPlantIncome(state) : CountZombieEconomy(state);
        const int opponentUnits = side == VSSide::Plants ? CountActiveZombies(state) : CountLivePlants(state);
        const int ownLaneUnits = side == VSSide::Plants ? CountPlantsInRow(state, targetRow) : CountZombiesInRow(state, targetRow);
        const int opponentLaneUnits = side == VSSide::Plants ? CountZombiesInRow(state, targetRow) : CountPlantsInRow(state, targetRow);
        const int totalLiveUnits = CountLivePlants(state) + CountActiveZombies(state);
        const int phase = ownEconomy < 3 && totalLiveUnits < 11 ? 0 : totalLiveUnits < 25 ? 1 : 2;
        const std::uint32_t deckSignature = DeckSignature(state, side);
        const std::array<int, 4> buckets = {
            StrategyBucket(ownEconomy),
            StrategyBucket(opponentUnits),
            StrategyBucket(ownLaneUnits),
            StrategyBucket(opponentLaneUnits),
        };

        int bestBonus = 0;
        for (const StrategyRule &rule : mRules) {
            if (rule.side != side || rule.seed != static_cast<std::uint16_t>(seed) || rule.phase != phase
                || (rule.deckSignature != 0 && rule.deckSignature != deckSignature)) {
                continue;
            }
            bool matches = true;
            for (std::size_t index = 0; index < buckets.size(); ++index) {
                matches = matches && (rule.buckets[index] < 0 || rule.buckets[index] == buckets[index]);
            }
            if (matches) {
                bestBonus = std::max(bestBonus, rule.bonus);
            }
        }
        return bestBonus;
    }
};

int StrategyBonus(const VSGameState &state, VSSide side, SeedType seed, int targetRow) {
    static StrategyDatabase database;
    return database.Bonus(state, side, seed, targetRow);
}

bool IsReadyCard(const VSCardState &card, int resource) {
    return card.seedType != static_cast<std::uint16_t>(SeedType::SEED_NONE) && !card.matchRestricted && card.active && !card.refreshing
        && card.refreshCounter <= 0 && card.cost <= resource;
}

int ReadyPlantAreaCounterCount(const VSGameState &state) {
    return static_cast<int>(std::count_if(state.seedBanks[0].begin(), state.seedBanks[0].end(), [&state](const VSCardState &card) {
        return IsAreaCounterSeed(static_cast<SeedType>(card.seedType)) && IsReadyCard(card, state.plantSun);
    }));
}

int PlantAreaCounterExposure(const VSGameState &state, int row) {
    const int readyCounters = ReadyPlantAreaCounterCount(state);
    const VSZombieState *closest = FindClosestZombie(state, row);
    if (readyCounters == 0 || closest == nullptr) {
        return 0;
    }

    const int zombieCount = CountZombiesInRow(state, row);
    const int stackCount = LargestZombieStackInRow(state, row);
    int score = 0;
    if (zombieCount >= 2) {
        score += 130 + (zombieCount - 2) * 90;
    }
    if (stackCount >= 2) {
        score += 150 + (stackCount - 2) * 120;
    }
    // Once a front reaches the plant half, its exact position is already a
    // legal Squash/Cherry target.  Do not make that trade easier for plants.
    if (closest->positionX < 760.0f) {
        score += 110;
    }
    return score * std::min(readyCounters, 2);
}

bool IsAreaCounterSeed(SeedType seed) {
    return seed == SeedType::SEED_SQUASH || seed == SeedType::SEED_CHERRYBOMB || seed == SeedType::SEED_JALAPENO
        || seed == SeedType::SEED_ICESHROOM || seed == SeedType::SEED_DOOMSHROOM;
}

bool IsZombieBreakthroughSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_BOBSLED:
        case SeedType::SEED_ZOMBONI:
        case SeedType::SEED_ZOMBIE_FOOTBALL:
        case SeedType::SEED_ZOMBIE_GARGANTUAR:
        case SeedType::SEED_ZOMBIE_GIGA_FOOTBALL:
        case SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR:
        case SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER:
            return true;
        default:
            return false;
    }
}

bool HasReadyZombieBreakthroughCard(const VSGameState &state) {
    return std::any_of(state.seedBanks[1].begin(), state.seedBanks[1].end(), [&state](const VSCardState &card) {
        return IsZombieBreakthroughSeed(static_cast<SeedType>(card.seedType)) && IsReadyCard(card, state.zombieBrains);
    });
}

bool IsHeavyZombieSeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_GARGANTUAR || seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR
        || seed == SeedType::SEED_ZOMBIE_GIGA_FOOTBALL || seed == SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER;
}

bool IsZombieGraveGuardSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_TRASHCAN:
        case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
        case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
        case SeedType::SEED_ZOMBIE_TALLNUT_HEAD:
        case SeedType::SEED_ZOMBIE_PAIL:
        case SeedType::SEED_ZOMBIE_NEWSPAPER:
        case SeedType::SEED_ZOMBIE_SUNDAY_EDITION:
        case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
            return true;
        default:
            return false;
    }
}

bool HasZombieGraveGuardInRow(const VSGameState &state, int row) {
    return HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_TRASHCAN)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_DOOR)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_WALLNUT_HEAD)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_TALLNUT_HEAD)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_PAIL)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_NEWSPAPER)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_SUNDAY_EDITION)
        || HasZombieTypeInRow(state, row, ZombieType::ZOMBIE_TRAFFIC_CONE);
}

} // namespace vsai::detail
