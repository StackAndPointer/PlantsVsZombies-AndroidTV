#include "ZombieCardRules.h"

#include <algorithm>

namespace vsai::detail {

int ZombieTemplateTacticalBonus(const ZombieTemplateProfile &profile, SeedType seed,
    const ZombieTemplateTacticalState &state) {
    const int economyCount = state.economyCount;
    const int activePressureRows = state.activePressureRows;
    const int attackCommitPressureRows = state.attackCommitPressureRows;
    const int zombiesInRow = state.zombiesInRow;
    const int rows = state.rows;
    const int peaHeadCount = state.peaHeadCount;
    const int plantCount = state.plantCount;
    const int economyValue = state.economyValue;
    const int sustainedOutput = state.sustainedOutput;
    const int areaCounterExposure = state.areaCounterExposure;
    const bool hasWallnut = state.hasWallnut;
    const bool graveUnderDirectFire = state.graveUnderDirectFire;
    const bool emptyRoute = zombiesInRow == 0;
    const bool openingWindow = economyCount >= 2 && economyCount <= rows + 2
        && activePressureRows < std::min(rows, 3);
    const bool conversionWindow = activePressureRows >= attackCommitPressureRows
        && areaCounterExposure < 140;
    const bool developedTarget = plantCount >= 2 || economyValue >= 80 || sustainedOutput >= 65 || hasWallnut;
    int bonus = 0;

    // Template tactics only refine candidates accepted by the placement,
    // counter, grave-protection, and lane-spread rules.
    if (profile.Has(ZombieTemplate::NewspaperScreenFootball)) {
        if (seed == SeedType::SEED_ZOMBIE_NEWSPAPER && openingWindow) bonus += emptyRoute ? 175 : -145;
        else if (seed == SeedType::SEED_ZOMBIE_PAIL && economyCount >= 3 && economyCount <= rows + 2) bonus += emptyRoute ? 145 : -125;
        else if (seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR && economyCount >= rows && graveUnderDirectFire && emptyRoute) bonus += 175;
        else if (seed == SeedType::SEED_ZOMBIE_GIGA_FOOTBALL && economyCount >= 6 && conversionWindow) bonus += emptyRoute && developedTarget ? 205 : -155;
    }
    if (profile.Has(ZombieTemplate::DogPeaFootball)) {
        if (seed == SeedType::SEED_ZOMBIE_DOGWALKER && openingWindow) bonus += emptyRoute ? 170 : -165;
        else if (seed == SeedType::SEED_ZOMBIE_PEA_HEAD && openingWindow && peaHeadCount < std::min(rows, 3)) bonus += emptyRoute ? 195 : -175;
        else if (seed == SeedType::SEED_ZOMBIE_TRAFFIC_CONE && economyCount >= 3 && peaHeadCount >= 2) bonus += emptyRoute ? 135 : -140;
        else if (seed == SeedType::SEED_ZOMBIE_FOOTBALL && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 190 : -160;
    }
    if (profile.Has(ZombieTemplate::NewspaperFootballPole)) {
        if (seed == SeedType::SEED_ZOMBIE_NEWSPAPER && openingWindow) bonus += emptyRoute ? 185 : -165;
        else if (seed == SeedType::SEED_ZOMBIE_NORMAL && economyCount >= 3 && economyCount <= rows + 2) bonus += emptyRoute ? 155 : -160;
        else if (seed == SeedType::SEED_ZOMBIE_FOOTBALL && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 170 : -145;
        else if (seed == SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 215 : -145;
    }
    if (profile.Has(ZombieTemplate::DancerRaid)) {
        if (seed == SeedType::SEED_ZOMBIE_DANCER && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 225 : -145;
        else if (seed == SeedType::SEED_ZOMBIE_PAIL && economyCount >= 5 && activePressureRows >= 1) bonus += emptyRoute ? 135 : -130;
        else if (seed == SeedType::SEED_ZOMBIE_JACKSON && economyCount >= 6 && conversionWindow) bonus += emptyRoute && developedTarget ? 245 : -175;
    }
    if (profile.Has(ZombieTemplate::PeaHeadRaid)) {
        if (seed == SeedType::SEED_ZOMBIE_PEA_HEAD && openingWindow && peaHeadCount < std::min(rows, 3)) bonus += emptyRoute ? 200 : -180;
        else if (seed == SeedType::SEED_ZOMBIE_SQUASH_HEAD && economyCount >= 3 && activePressureRows >= 1) bonus += emptyRoute && developedTarget ? 190 : -135;
        else if (seed == SeedType::SEED_ZOMBIE_BUNGEE && economyCount >= 5 && conversionWindow) bonus += developedTarget ? 155 : -130;
    }
    if (profile.Has(ZombieTemplate::PeaHeadDancerRaid)) {
        if (seed == SeedType::SEED_ZOMBIE_PEA_HEAD && openingWindow && peaHeadCount < std::min(rows, 3)) bonus += emptyRoute ? 195 : -175;
        else if (seed == SeedType::SEED_ZOMBIE_DANCER && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 205 : -145;
        else if (seed == SeedType::SEED_ZOMBIE_JALAPENO_HEAD && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 215 : -165;
    }
    if (profile.Has(ZombieTemplate::MoundPeaZomblobFootball)) {
        if (seed == SeedType::SEED_ZOMBIE_PEA_HEAD && openingWindow && peaHeadCount < std::min(rows, 3)) bonus += emptyRoute ? 185 : -170;
        else if (seed == SeedType::SEED_ZOMBIE_MOUND && economyCount >= 2 && economyCount <= rows + 3) bonus += graveUnderDirectFire ? -175 : 215;
        else if (seed == SeedType::SEED_ZOMBIE_ZOMBLOB && economyCount >= 5 && conversionWindow) bonus += emptyRoute && peaHeadCount >= 2 && developedTarget ? 205 : -160;
        else if (seed == SeedType::SEED_ZOMBIE_GIGA_FOOTBALL && economyCount >= 7 && conversionWindow) bonus += emptyRoute && developedTarget ? 190 : -155;
    }
    if (profile.Has(ZombieTemplate::SundayLadderRaid)) {
        if (seed == SeedType::SEED_ZOMBIE_NORMAL && economyCount >= 1 && economyCount <= rows + 1) bonus += emptyRoute ? 180 : -175;
        else if (seed == SeedType::SEED_ZOMBIE_LADDER && economyCount >= 3 && (hasWallnut || developedTarget)) bonus += emptyRoute ? 175 : -130;
        else if (seed == SeedType::SEED_ZOMBIE_SUNDAY_EDITION && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 225 : -165;
        else if (seed == SeedType::SEED_ZOMBIE_JALAPENO_HEAD && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 195 : -150;
    }
    if (profile.Has(ZombieTemplate::MoundNewspaperZamboni)) {
        if (seed == SeedType::SEED_ZOMBIE_IMP && economyCount >= 3 && economyCount <= rows + 2) bonus += emptyRoute ? 180 : -170;
        else if (seed == SeedType::SEED_ZOMBIE_MOUND && economyCount >= 3 && economyCount <= rows + 3) bonus += graveUnderDirectFire ? -170 : 200;
        else if (seed == SeedType::SEED_ZOMBIE_NEWSPAPER && economyCount >= 3 && economyCount <= rows + 3) bonus += emptyRoute ? 155 : -145;
        else if (seed == SeedType::SEED_ZOMBONI && economyCount >= 5 && conversionWindow) bonus += emptyRoute && developedTarget ? 205 : -170;
    }
    if (profile.Has(ZombieTemplate::MoundTallnutGuard)) {
        if (seed == SeedType::SEED_ZOMBIE_TRAFFIC_CONE && openingWindow) bonus += emptyRoute ? 165 : -155;
        else if (seed == SeedType::SEED_ZOMBIE_MOUND && economyCount >= 3 && economyCount <= rows + 3) bonus += graveUnderDirectFire ? -180 : 220;
        else if (seed == SeedType::SEED_ZOMBIE_TALLNUT_HEAD && economyCount >= 5 && (graveUnderDirectFire || sustainedOutput >= 80)) bonus += emptyRoute ? 205 : -130;
        else if (seed == SeedType::SEED_ZOMBIE_PAIL && economyCount >= 3 && activePressureRows >= 1) bonus += emptyRoute ? 125 : -115;
    }
    return bonus;
}

} // namespace vsai::detail
