#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_STRATEGY_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_STRATEGY_H

#include "VSActionAIPlacement.h"

#include <array>
#include <memory>

namespace vsai::detail {

int StrategyBucket(int value);
int StrategyBonus(const VSGameState &state, VSSide side, SeedType seed, int targetRow);
bool IsAreaCounterSeed(SeedType seed);
int ReadyPlantAreaCounterCount(const VSGameState &state);
int PlantAreaCounterExposure(const VSGameState &state, int row);
bool IsZombieBreakthroughSeed(SeedType seed);
bool HasReadyZombieBreakthroughCard(const VSGameState &state);
bool IsHeavyZombieSeed(SeedType seed);
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

    VSAction MakeShovelAction(VSGridPosition target, std::uint32_t tick) {
        return {
            .side = VSSide::Plants,
            .kind = VSActionKind::Shovel,
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

std::unique_ptr<IVSAgent> CreatePlantAI();
std::unique_ptr<IVSAgent> CreateZombieAI();

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_STRATEGY_H
