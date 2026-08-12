#include "VSActionAILanePolicy.h"

namespace vsai::detail {

ZombieLanePolicy EvaluateZombieLanePolicy(const VSGameState &state, int row) {
    ZombieLanePolicy policy{};
    if (row < 0 || row >= state.rows) {
        return policy;
    }

    policy.deploymentBlocked = IsMowerInMotion(state, row) || HasZombieInHomeColumn(state, row) || IsMowerAboutToTrigger(state, row);
    policy.hasLiveTarget = HasLiveZombieTargetInRow(state, row);
    policy.mowerless = row < static_cast<int>(state.mowerAvailable.size())
        && !state.mowerAvailable[static_cast<std::size_t>(row)] && !IsMowerInMotion(state, row);
    policy.strongMowerlessPlantLane = IsMowerlessStrongPlantLane(state, row);
    policy.conversionRoute = !policy.deploymentBlocked && policy.mowerless && policy.hasLiveTarget;
    policy.allowsAttack = !policy.deploymentBlocked && policy.hasLiveTarget
        && (!policy.strongMowerlessPlantLane || policy.conversionRoute);
    policy.allowsEconomy = !policy.deploymentBlocked && policy.hasLiveTarget
        && (!policy.strongMowerlessPlantLane || AllMowersSpent(state));
    return policy;
}

int MowerlessLaneAttackScoreBonus(const VSGameState &state, const ZombieLanePolicy &policy, int zombieCount) {
    if (!policy.conversionRoute) {
        return 0;
    }
    return (zombieCount > 0 ? 1420 : 1280) + (AllMowersSpent(state) ? 240 : 0);
}

int MowerlessLaneDistributionAdjustment(const ZombieLanePolicy &policy, int zombieCount) {
    if (!policy.conversionRoute) {
        return 0;
    }
    if (zombieCount == 0) {
        return 620;
    }
    if (zombieCount == 1) {
        return 180;
    }
    return -15 - (zombieCount - 1) * 45;
}

int MowerlessLaneCommitmentBonus(const ZombieLanePolicy &policy, int zombieCount) {
    if (!policy.conversionRoute) {
        return 0;
    }
    return zombieCount == 0 ? 1450 : 1700;
}

} // namespace vsai::detail
