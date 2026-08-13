#include "PlantAI.h"

namespace vsai::detail {

PlantDecisionResult PlantAIPlanning::TryEmergencyPolicy(const VSGameState &state, const PlantDecisionContext &context) {
    if (context.zamboniRow >= 0) {
        if (std::optional<VSAction> action = TrySpikeweed(state, context.zamboniRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
    }
    if (context.impactThreatRow >= 0) {
        if (std::optional<VSAction> action = TryImpactDistraction(state, context.impactThreatRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
    }
    if (context.hasActiveZombie) {
        if (std::optional<VSAction> action = TryWakeSleepingDoomshroom(state)) {
            return {.handled = true, .action = action};
        }
        if (std::optional<VSAction> action = TryBestAshCounter(state, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
    }
    if (std::optional<VSAction> action = TryUmbrellaDefense(state, context.protectedSun)) {
        return {.handled = true, .action = action};
    }
    if (context.hasActiveZombie && !context.mowerlessThirdColumnEmergency) {
        if (std::optional<VSAction> action = TryHypnoshroom(state, context.counterRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
        if (std::optional<VSAction> action = TryPotatoMine(state, context.firepowerRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
        if (context.metalZombieDeck) {
            if (std::optional<VSAction> action = TryMagnetShroom(state, context.firepowerRow, context.protectedSun)) {
                return {.handled = true, .action = action};
            }
        }
        if (std::optional<VSAction> action = TrySnowpeaBonkPressure(state, context.firepowerRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
        if (std::optional<VSAction> action = TryStarfruitChomperPressure(state, context.firepowerRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
        if (std::optional<VSAction> action = TryKernelCeleryPressure(state, context.firepowerRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
        if (std::optional<VSAction> action = TryCactusSpikeweedPressure(state, context.firepowerRow, context.protectedSun)) {
            return {.handled = true, .action = action};
        }
    }
    if (context.hasActiveZombie && context.impPearThreat && !HasPlantTypeInRow(state, SeedType::SEED_IMP_PEAR, context.counterRow)) {
        if (std::optional<VSAction> action = TryCounterPlant(state, SeedType::SEED_IMP_PEAR, context.counterRow, context.counterFirstColumn)) {
            return {.handled = true, .action = action};
        }
    }
    if (context.squashThreat && context.counterClosest != nullptr
        && !HasPlantTypeInRow(state, SeedType::SEED_SQUASH, context.counterRow)) {
        if (std::optional<VSAction> action = TryCounterPlant(state, SeedType::SEED_SQUASH, context.counterRow, context.counterFirstColumn)) {
            return {.handled = true, .action = action};
        }
    }
    if (context.hasActiveZombie) {
        if (std::optional<VSAction> action = TryIcebergLettuce(state, context.counterRow, context.protectedSun,
                context.mowerlessThirdColumnEmergency)) {
            return {.handled = true, .action = action};
        }
        if (context.rangedZombieDeck) {
            if (std::optional<VSAction> action = TryPumpkinShell(state, context.counterRow, context.protectedSun)) {
                return {.handled = true, .action = action};
            }
        }
        if (HasPlantTypeInRow(state, SeedType::SEED_ICEBERG_LETTUCE, context.counterRow)
            && ShouldDeployWallnut(state, context.counterRow)) {
            if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_WALLNUT, context.counterRow, 3, 5)) {
                return {.handled = true, .action = action};
            }
        }
    }
    if (!context.mowerlessThirdColumnEmergency) {
        return {};
    }
    if (ShouldDeployWallnut(state, context.counterRow)) {
        if (std::optional<VSAction> action = TryPlant(state, SeedType::SEED_WALLNUT, context.counterRow, 0, 5)) {
            return {.handled = true, .action = action};
        }
    }
    if (std::optional<VSAction> action = TryPumpkinShell(state, context.counterRow, context.protectedSun)) {
        return {.handled = true, .action = action};
    }
    if (std::optional<VSAction> action = TrySustainedOutputPlant(state, context.counterRow, context.protectedSun, true, true, true)) {
        return {.handled = true, .action = action};
    }
    return {.handled = true};
}

} // namespace vsai::detail
