#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_AI_PLANNING_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_AI_PLANNING_H

#include "../VSActionAIStrategy.h"

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

    static bool IsTargetedSeed(std::uint16_t seed);
    static bool IsEconomySeed(SeedType seed);
    static bool IsFrontlineProbeSeed(SeedType seed);
    static bool IsFastAttackSeed(SeedType seed);
    int HeavyZombieReserve(const VSGameState &state) const;
    bool HasReadyFrontlineProbe(const VSGameState &state) const;
    bool HasReadyEarlyHeavyCommit(const VSGameState &state, int economyCount, int activePressureRows) const;
    bool IsEarlyHeavyCommitCard(const VSGameState &state, SeedType seed, int economyCount, int activePressureRows) const;
    std::optional<VSAction> TryBuildEconomy(const VSGameState &state, int row);
    static int GraveGuardPriority(SeedType seed);
    std::optional<VSAction> TryProtectEconomy(const VSGameState &state, int row);
    std::optional<VSAction> TryCounterLobbedGravePressure(const VSGameState &state, int row);
    int LeastCommittedZombieRow(const VSGameState &state) const;
    static int BungeeTargetScore(const VSGameState &state, const VSPlantState &plant, int row);
    static int CardScore(const VSCardState &card, const VSGameState &state, int targetRow, int economyCount, int effectiveCost);

public:
    void Reset() override;
};

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_AI_PLANNING_H
