#ifndef PVZ_LAWN_VS_ACTION_AI_INTERNAL_H
#define PVZ_LAWN_VS_ACTION_AI_INTERNAL_H

#include "PvZ/Lawn/VSActionAIDecision.h"
#include "PvZ/Lawn/Common/ConstEnums.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace vsai::detail {

struct PlantLaneAssessment {
    int row = 0;
    int danger = 0;
    int rawDanger = 0;
    int defense = 0;
    int plantCount = 0;
    bool hasHeavy = false;
    bool hasFast = false;
    const VSZombieState *closest = nullptr;
};

// Snapshot-only combat estimate used by both agents.  DPS is expressed in
// PvZ damage per second; the estimate intentionally values time before the
// closest zombie reaches the plant half of the lawn, not just unit counts.
struct PlantLaneFirepower {
    int row = 0;
    int dps = 0;
    int incomingHealth = 0;
    int nearHealth = 0;
    int closestDistance = 0;
    int secondsToContact = 0;
    int damageBeforeContact = 0;
    int deficit = 0;
    bool canHold = true;
};

bool IsDeadOrOutside(const VSPlantState &plant);
bool HasPlantAt(const VSGameState &state, VSGridPosition position);
bool HasPlantTypeAt(const VSGameState &state, SeedType seedType, VSGridPosition position);
bool HasGridItemAt(const VSGameState &state, VSGridPosition position);
const VSZombieState *FindClosestZombie(const VSGameState &state, int row = -1);
int CountPlantsInRow(const VSGameState &state, int row);
int CountZombiesInRow(const VSGameState &state, int row);
int CountActiveZombies(const VSGameState &state);
int CountActiveZombieRows(const VSGameState &state);
int CountPlantType(const VSGameState &state, SeedType seedType);
bool HasPlantTypeInRow(const VSGameState &state, SeedType seedType, int row);
bool IsHeavyZombie(std::uint16_t zombieType);
bool IsFastZombie(std::uint16_t zombieType);
bool IsDecisiveCounterZombie(std::uint16_t zombieType);
bool HasZombieTypeInRow(const VSGameState &state, int row, ZombieType zombieType);
int LargestZombieStackInRow(const VSGameState &state, int row);
int LargestCherryBombClusterInRow(const VSGameState &state, int row);
int ZombieThreatWeight(std::uint16_t zombieType);
int CounterPressureScoreInRow(const VSGameState &state, int row);
int MostUrgentCounterRow(const VSGameState &state);
int ZombieFrontlineValueInRow(const VSGameState &state, int row);
int MostValuableZombieFrontRow(const VSGameState &state);
int ZombiePressureInRow(const VSGameState &state, int row);
int PlantDefenseValue(const VSPlantState &plant);
int PlantDamagePerSecond(SeedType seedType);
PlantLaneFirepower AssessPlantLaneFirepower(const VSGameState &state, int row);
int PlantLaneFirepowerDeficit(const VSGameState &state, int row);
PlantLaneAssessment AssessPlantLane(const VSGameState &state, int row);
PlantLaneAssessment MostThreatenedPlantLane(const VSGameState &state);
bool IsPlantEconomySeed(const VSGameState &state, std::uint16_t seedType);
int LeastDevelopedPlantRow(const VSGameState &state);
int PlantValueScore(const VSPlantState &plant);
bool IsPlantCombatSeed(std::uint16_t seedType);
bool IsSustainedOutputSeed(SeedType seedType);
int SustainedOutputValue(SeedType seedType);
int CountSustainedOutputPlants(const VSGameState &state);
int SustainedOutputScoreInRow(const VSGameState &state, int row);
int PlantEconomyValueInRow(const VSGameState &state, int row);
bool HasSustainedOutputSeed(const VSGameState &state);
bool IsZombieEconomyItem(std::uint16_t gridItemType);
int EstimatedEconomyMaxHealth(const VSGridItemState &item);
int StraightProjectileThreatToEconomy(const VSPlantState &plant, const VSGridItemState &economy);
int PlantThreatToEconomy(const VSPlantState &plant, const VSGridItemState &economy);
int StraightProjectileThreatScore(const VSGameState &state, int row);
int GraveThreatScore(const VSGameState &state, int row);
int ZombieEconomyAssetValue(const VSGridItemState &item);
int ZombieEconomyAttackOpportunity(const VSGameState &state, int row);
int SeedEconomyPressureOpportunity(const VSGameState &state, SeedType seed, int row);
int MostVulnerableZombieEconomyRow(const VSGameState &state);
int MostThreatenedEconomyRow(const VSGameState &state);
int LeastThreatenedEconomyRow(const VSGameState &state);
int PlantLaneWeaknessScore(const VSGameState &state, int row);
int EconomyPlantsInRow(const VSGameState &state, int row);
int ZombieLaneAttackScore(const VSGameState &state, int row);
int MostVulnerablePlantRow(const VSGameState &state);
VSGridPosition FindPlantCellInColumns(const VSGameState &state, int preferredRow, int firstColumn, int lastColumn);
VSGridPosition FindPlantCellInExactRow(const VSGameState &state, int row, int firstColumn, int lastColumn);
bool IsIncomeRowSafe(const VSGameState &state, int row);
bool IsRangedOutputTradeUnfavorable(const VSGameState &state, int row);
VSGridPosition FindSafeIncomeCell(const VSGameState &state, int preferredRow);
int ZombiePlacementColumn(SeedType seed);
VSGridPosition FindZombieCell(const VSGameState &state, SeedType seed, int row);
VSGridPosition FindZombieEconomyCell(const VSGameState &state, int preferredRow);
VSGridPosition FindZombieMoundCell(const VSGameState &state, int row);
int MoundUpgradeCostAt(const VSGameState &state, VSGridPosition position);
bool IsReadyCard(const VSCardState &card, int resource);
bool IsCardReadyForZombieTarget(const VSCardState &card, const VSGameState &state, VSGridPosition target);
int CountZombieEconomy(const VSGameState &state);
int HeavyZombieEconomyThreshold(const VSGameState &state);
int StrategyBucket(int value);
int CountLivePlants(const VSGameState &state);
int CountPlantIncome(const VSGameState &state);
int StrategyBonus(const VSGameState &state, VSSide side, SeedType seed, int targetRow);
bool IsAreaCounterSeed(SeedType seed);
int ReadyPlantAreaCounterCount(const VSGameState &state);
int PlantAreaCounterExposure(const VSGameState &state, int row);
bool HasReadyZombieGraveGuard(const VSGameState &state);
bool IsZombieBreakthroughSeed(SeedType seed);
bool HasReadyZombieBreakthroughCard(const VSGameState &state);
bool IsHeavyZombieSeed(SeedType seed);
bool IsLateGameHeavyZombieSeed(SeedType seed);
bool IsZombieGraveGuardSeed(SeedType seed);
bool HasZombieGraveGuardInRow(const VSGameState &state, int row);

class BuiltinVSAgent : public IVSAgent {
protected:
    std::uint16_t mSequence = 0;
    std::array<std::uint8_t, 32> mBlockedSlots{};

    void AdvanceBlockedSlots() {
        for (std::uint8_t &blocked : mBlockedSlots) {
            if (blocked > 0) {
                --blocked;
            }
        }
    }

    bool IsSlotBlocked(std::uint8_t slot) const {
        return slot < mBlockedSlots.size() && mBlockedSlots[slot] != 0;
    }

    VSAction MakePlayAction(VSSide side, const VSCardState &card, VSGridPosition target, std::uint32_t tick) {
        return {
            .side = side,
            .kind = VSActionKind::PlaySeed,
            .seedSlot = card.slot,
            .expectedSeedType = card.seedType,
            .target = target,
            .notBeforeTick = tick,
            .expiresAtTick = tick + 120,
            .sequence = ++mSequence,
        };
    }

public:
    void Reset() override {
        mSequence = 0;
        mBlockedSlots.fill(0);
    }

    void OnActionResult(const VSAction &action, VSActionResult result) override {
        if (result == VSActionResult::RejectedInvalidTarget || result == VSActionResult::RejectedUnsupported || result == VSActionResult::RejectedCardUnavailable) {
            if (action.seedSlot < mBlockedSlots.size()) {
                mBlockedSlots[action.seedSlot] = 4;
            }
        }
    }
};

std::unique_ptr<IVSAgent> CreatePlantVSAgent();
std::unique_ptr<IVSAgent> CreateZombieVSAgent();

} // namespace vsai::detail

#endif // PVZ_LAWN_VS_ACTION_AI_INTERNAL_H
