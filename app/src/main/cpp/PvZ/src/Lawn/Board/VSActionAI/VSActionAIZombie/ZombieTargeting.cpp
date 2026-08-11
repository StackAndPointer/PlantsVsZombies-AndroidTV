#include "ZombieAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include "PvZ/Lawn/Board/Plant.h"

namespace vsai::detail {

int ZombieAIPlanning::LeastCommittedZombieRow(const VSGameState &state) const {
    int bestRow = 0;
    int bestScore = std::numeric_limits<int>::max();
    for (int row = 0; row < state.rows; ++row) {
        int economy = 0;
        for (const VSGridItemState &item : state.gridItems) {
            economy += !item.dead && item.position.row == row && IsZombieEconomyItem(item.gridItemType) ? 1 : 0;
        }
        // Strategy data is only a tie-break after the live-board economy
        // and grave-threat checks. It can guide an otherwise equivalent
        // grave position, but cannot force a threatened or stacked row.
        const int score = economy * 130 + CountZombiesInRow(state, row) * 85 + GraveThreatScore(state, row) * 2
            - StrategyBonus(state, VSSide::Zombies, SeedType::SEED_ZOMBIE_GRAVESTONE, row) * 2;
        if (score < bestScore) {
            bestScore = score;
            bestRow = row;
        }
    }
    return bestRow;
}

int ZombieAIPlanning::BungeeTargetScore(const VSGameState &state, const VSPlantState &plant, int row) {
    const SeedType seed = static_cast<SeedType>(plant.seedType);
    // Bungee is an economic strike. It must not spend its long cooldown
    // on a Wall-nut, Pumpkin or Umbrella while a real carry is present.
    switch (seed) {
        case SeedType::SEED_WALLNUT:
        case SeedType::SEED_TALLNUT:
        case SeedType::SEED_PUMPKINSHELL:
        case SeedType::SEED_UMBRELLA:
        case SeedType::SEED_GARLIC:
        case SeedType::SEED_SPIKEWEED:
        case SeedType::SEED_SPIKEROCK:
        case SeedType::SEED_IMP_PEAR:
        case SeedType::SEED_POTATOMINE:
        case SeedType::SEED_SQUASH:
        case SeedType::SEED_CHERRYBOMB:
        case SeedType::SEED_JALAPENO:
        case SeedType::SEED_CHILLY_PEPPER:
        case SeedType::SEED_ICESHROOM:
        case SeedType::SEED_DOOMSHROOM:
        case SeedType::SEED_ICEBERG_LETTUCE:
        case SeedType::SEED_HYPNOSHROOM:
        case SeedType::SEED_GRAVEBUSTER:
        case SeedType::SEED_BLOVER:
        case SeedType::SEED_TANGLEKELP:
            return std::numeric_limits<int>::min();
        default:
            break;
    }

    // Board::FindUmbrellaPlant rejects every target in this 3x3 area.
    // Do not consume Bungee's cooldown merely to trigger a reflection.
    if (IsPlantProtectedByUmbrella(state, plant.position)) {
        return std::numeric_limits<int>::min();
    }

    const int plantCost = std::max(0, Plant::GetCost(seed, SeedType::SEED_NONE));

    if (plantCost < 100) {
        return std::numeric_limits<int>::min();
    }

    int score = plantCost * 5 + PlantValueScore(plant) + static_cast<int>(plant.position.col) * 8;
    if (IsPlantCombatSeed(plant.seedType)) {
        score += 260;
    }
    score += PlantLaneWeaknessScore(state, row) / 3;
    return score;
}

} // namespace vsai::detail
