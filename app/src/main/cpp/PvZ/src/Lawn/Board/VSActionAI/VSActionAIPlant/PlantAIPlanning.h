#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_PLANNING_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_PLANNING_H

#include "../VSActionAIStrategy.h"

#include <limits>
#include <optional>

namespace vsai::detail {

bool IsLobbedOutputSeed(SeedType seed);

class PlantAIPlanning : public BuiltinVSAgent {
protected:
    struct AshTarget {
        VSGridPosition position{-1, -1};
        int hitCount = 0;
        int totalHealth = 0;
        int highValueCount = 0;
        int pailCount = 0;
        float frontMostX = std::numeric_limits<float>::max();
        bool mowerlessThirdColumn = false;
        bool mowerlessHomeColumn = false;
        int score = std::numeric_limits<int>::min();
    };

    static bool IsDaytimeCoffeeMushroom(SeedType seed);
    static bool IsSquashClusterZombie(std::uint16_t zombieType);
    static bool IsSquashHighValueZombie(std::uint16_t zombieType);
    static int LargestSquashTargetStackInRow(const VSGameState &state, int row);
    const VSCardState *FindReadyCard(const VSGameState &state, SeedType seedType) const;
    std::optional<VSAction> TryBlover(const VSGameState &state, int preferredRow);
    std::optional<VSAction> TryEvadeJalapenoHead(const VSGameState &state);
    int EffectivePlantPlayCost(const VSGameState &state, const VSCardState &card) const;
    static bool IsInstantCounterSeed(SeedType seedType);
    std::optional<VSAction> TryPlantInRange(const VSGameState &state, SeedType seedType, int row, int firstColumn, int lastColumn,
        bool requireExactRow);
    std::optional<VSAction> TryPlant(const VSGameState &state, SeedType seedType, int row, int firstColumn, int lastColumn);
    std::optional<VSAction> TryPlantExactRow(const VSGameState &state, SeedType seedType, int row, int firstColumn, int lastColumn);
    std::optional<VSAction> TryRemoveLadderedNut(const VSGameState &state);
    std::optional<VSAction> TryCounterPlant(const VSGameState &state, SeedType seedType, int row, int firstColumn);

    static int ZombieColumn(const VSZombieState &zombie);
    static int ZombieEffectiveHealth(const VSZombieState &zombie);
    static bool IsHypnoshroomTarget(const VSZombieState &zombie);
    static int PotatoMineArmingLead(const VSZombieState &zombie);
    static bool IsSquashTargetZombie(const VSZombieState &zombie);
    AshTarget FindBestAshTarget(const VSGameState &state, SeedType seedType) const;
    static bool IsAshTargetWorthPlaying(const VSGameState &state, SeedType seedType, const AshTarget &target);
    std::optional<VSAction> TryAshCounter(const VSGameState &state, SeedType seedType, int protectedSun);
    std::optional<VSAction> TryPotatoMine(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySnowpeaBonkPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryStarfruitChomperPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryKernelCeleryPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryMagnetShroom(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryHypnoshroom(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryWakeSleepingDoomshroom(const VSGameState &state);
    std::optional<VSAction> TryStarfruitGarlicFormation(const VSGameState &state, int protectedSun);
    std::optional<VSAction> TryIcebergLettuce(const VSGameState &state, int row, int protectedSun, bool forceEmergencyControl = false);
    std::optional<VSAction> TryTorchwoodSupport(const VSGameState &state, int protectedSun);
    std::optional<VSAction> TryIncomePlant(const VSGameState &state, int row, int protectedSun);
    std::optional<VSAction> TrySunshroomFiller(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryGraveBuster(const VSGameState &state, int protectedSun);

    VSGridPosition FindSustainedOutputCell(const VSGameState &state, SeedType seed, int row) const;
    bool HasReadySustainedOutputCard(const VSGameState &state, int protectedSun) const;
    std::optional<VSAction> TryRecycleIncomeForOutput(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySustainedOutputPlant(const VSGameState &state, int row, int protectedSun, bool allowLowCostCombat = false,
        bool requirePreferredRow = false, bool allowEmergencyTrade = false);
    bool HasIncomeSeed(const VSGameState &state) const;
    bool HasSunshroomSeed(const VSGameState &state) const;
    bool HasEconomyPressurePlan(const VSGameState &state) const;
    int EconomyPressureIncomeTarget(const VSGameState &state) const;
    std::optional<VSAction> TryBoomerangControlPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryThreepeaterPuffFormation(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySnowpeaPuffMagnetPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryPeaPuffTempoOpening(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryPeaCeleryAshTempo(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySporePuffTempoPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryPeaCabbageTorchTempo(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySnowpeaBonkFormation(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryPeaDoomTempoPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryStarfruitCrossfireFormation(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryCactusSpikeweedCore(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryKernelCeleryFormation(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryRepeaterCeleryTempo(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryMelonMineTempo(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryRepeaterTempoPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySporeShellPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryFumeDoomPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryMelonScaredySupport(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryScaredyMelonSupport(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryScaredyPuffDoomPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryStarfruitPuffPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryPeaPuffPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySporePuffPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryWakeableMushroomOutput(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TryWakeSleepingMushroom(const VSGameState &state, int preferredRow);

    std::optional<VSAction> TryPumpkinShell(const VSGameState &state, int row, int protectedSun);
    bool ShouldDeployWallnut(const VSGameState &state, int row) const;
    bool ShouldYieldLaneToMower(const VSGameState &state, int row) const;
    std::optional<VSAction> TrySpikeweed(const VSGameState &state, int row, int protectedSun);
    std::optional<VSAction> TryCactusSpikeweedPressure(const VSGameState &state, int preferredRow, int protectedSun);
    std::optional<VSAction> TrySquashHeadDistraction(const VSGameState &state, int row, int protectedSun);
    int AreaCounterReserve(const VSGameState &state) const;
    std::optional<VSAction> TryFallbackPlant(const VSGameState &state, const PlantLaneAssessment &danger, int buildRow);
};

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_PLANNING_H
