#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_AI_PLANNING_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_AI_PLANNING_H

#include "ZombieDecisionContext.h"

#include <array>
#include <optional>

namespace vsai::detail {

class ZombieAIPlanning : public BuiltinVSAgent {
protected:
    static bool HasLobbedPlantInRow(const VSGameState &state, int row);
    const VSCardState *FindReadyCard(const VSGameState &state, SeedType seedType) const;

    int mLastAttackRow = -1;
    int mLastPressureEconomyCount = -1;
    std::array<std::uint8_t, 6> mLaneAttackCooldown{};

    int HeavyZombieReserve(const VSGameState &state) const;
    bool HasReadyFrontlineProbe(const VSGameState &state) const;
    bool HasReadyEarlyHeavyCommit(const VSGameState &state, const ZombieDecisionContext &context) const;
    std::optional<VSAction> TryTemplateSundayRelease(const VSGameState &state, const ZombieDecisionContext &context);
    bool IsEarlyHeavyCommitCard(const VSGameState &state, SeedType seed, const ZombieDecisionContext &context) const;
    std::optional<VSAction> TryBuildEconomy(const VSGameState &state, int row);
    std::optional<VSAction> TryProtectEconomy(const VSGameState &state, int row, bool force = false);
    std::optional<VSAction> TryCounterLobbedGravePressure(const VSGameState &state,
        const ZombieDecisionContext &context, int row);
    int LeastCommittedZombieRow(const VSGameState &state) const;
    static int BungeeTargetScore(const VSGameState &state, const VSPlantState &plant, int row);
    static int CardScore(const VSCardState &card, const VSGameState &state, const ZombieDecisionContext &context,
        int targetRow, int effectiveCost);

public:
    void Reset() override;
};

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_AI_PLANNING_H
