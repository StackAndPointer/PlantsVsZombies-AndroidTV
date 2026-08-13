#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_H

#include "PlantAIPlanning.h"

namespace vsai::detail {

class PlantAI final : public PlantAIPlanning {
    PlantDecisionResult TryOpeningEconomyPhase(const VSGameState &state);
    std::optional<VSAction> TryImmediateMaintenancePhase(const VSGameState &state);
    PlantDecisionResult TryOpeningOutputPhase(const VSGameState &state, const PlantDecisionContext &context);
    PlantDecisionResult TryTemplatePressurePhase(const VSGameState &state, const PlantDecisionContext &context);
    PlantDecisionResult TryEconomyConversionPhase(const VSGameState &state, const PlantDecisionContext &context);
    PlantDecisionResult TryLaneDefensePhase(const VSGameState &state, const PlantDecisionContext &context);
    PlantDecisionResult TryFallbackPhase(const VSGameState &state, const PlantDecisionContext &context);

public:
    std::optional<VSAction> Decide(const VSGameState &state) override;
};

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_H
