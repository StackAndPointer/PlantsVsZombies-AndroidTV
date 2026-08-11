#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_H

#include "PlantAIPlanning.h"

namespace vsai::detail {

class PlantAI final : public PlantAIPlanning {
public:
    std::optional<VSAction> Decide(const VSGameState &state) override;
};

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_PLANT_AI_H
