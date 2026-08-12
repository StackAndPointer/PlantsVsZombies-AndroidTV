#include "ZombieAI.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include "PvZ/Lawn/Board/Plant.h"

namespace vsai::detail {
int ZombieAIPlanning::CardScore(const VSCardState &card, const VSGameState &state,
    const ZombieDecisionContext &context, int targetRow, int effectiveCost) {
    const SeedType seed = static_cast<SeedType>(card.seedType);
    const ZombieTempoPolicy &tempo = context.tempo;
    const int economyCount = context.economyCount;
    const bool hasPlants = std::any_of(state.plants.begin(), state.plants.end(), [](const VSPlantState &plant) { return !IsDeadOrOutside(plant); });
    const bool hasSnowPea = HasPlantTypeInRow(state, SeedType::SEED_SNOWPEA, targetRow);
    const bool hasBonkChoy = HasPlantTypeInRow(state, SeedType::SEED_BONK_CHOY, targetRow);
    const bool hasWallnut = HasPlantTypeInRow(state, SeedType::SEED_WALLNUT, targetRow) || HasPlantTypeInRow(state, SeedType::SEED_TALLNUT, targetRow);
    const bool hasPumpkinShell = HasPlantTypeInRow(state, SeedType::SEED_PUMPKINSHELL, targetRow);
    const int plantCount = CountPlantsInRow(state, targetRow);
    const int zombieCount = CountZombiesInRow(state, targetRow);
    const int graveProjectileThreat = StraightProjectileThreatScore(state, targetRow);
    const int lobbedProjectileThreat = LobbedProjectileThreatScore(state, targetRow);
    const bool hasLobbedPlant = ZombieAIPlanning::HasLobbedPlantInRow(state, targetRow);
    const int graveScreenDeficit = ZombieGraveScreenDeficit(state, targetRow);
    const bool hasGraveGuard = HasZombieGraveGuardInRow(state, targetRow);
    const int economyTarget = context.economyTarget;
    const int heavyEconomyThreshold = context.heavyEconomyThreshold;
    const int sustainedOutput = SustainedOutputScoreInRow(state, targetRow);
    const int economyValue = PlantEconomyValueInRow(state, targetRow);
    const PlantLaneAssessment targetLane = AssessPlantLane(state, targetRow);
    const int areaCounterExposure = PlantAreaCounterExposure(state, targetRow);
    const bool plantHasMagnet = HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_MAGNETSHROOM)
        || CountPlantType(state, SeedType::SEED_MAGNETSHROOM) > 0;
    const bool plantHasPeaCarry = HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_PEASHOOTER)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_REPEATER) || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_THREEPEATER);
    const bool plantHasShortPult = HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_FUMESHROOM)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_SPORESHROOM) || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_PUFFSHROOM);
    const bool plantHasLobbedCard = HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_CABBAGEPULT)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_KERNELPULT) || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_MELONPULT)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_WINTERMELON) || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_SPORESHROOM)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_FUMESHROOM) || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_PUFFSHROOM);
    const bool plantHasNutCard = HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_WALLNUT)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_PUMPKINSHELL);
    const bool plantHasHighValueCarryCard = HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_MELONPULT)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_SPORESHROOM) || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_STARFRUIT)
        || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_REPEATER) || HasActiveDeckCard(state, VSSide::Plants, SeedType::SEED_THREEPEATER);
    const ZombieTemplateProfile &profile = context.templateProfile;
    const bool fastPressureTemplate = profile.fastPressure;
    const bool rangedSiegeTemplate = profile.rangedSiege;
    const bool sundayPressureTemplate = profile.sundayPressure;
    const bool zamboniPoleTemplate = profile.Has(ZombieTemplate::ZamboniPole);
    const bool peaHeadGiantTemplate = profile.Has(ZombieTemplate::PeaHeadGiant);
    const bool impSledSundayTemplate = profile.Has(ZombieTemplate::ImpSledSunday);
    const bool armoredNormalRushTemplate = profile.Has(ZombieTemplate::ArmoredNormalRush);
    // Replay-specific archetypes only refine a legal candidate's score below.
    // They deliberately retain the general economy, anti-pult and lane-spread
    // checks so a recorded opening cannot turn into an unconditional script.
    const bool newspaperDiggerGigaTemplate = profile.Has(ZombieTemplate::NewspaperDiggerGiga);
    const bool newspaperSledDiggerGigaTemplate = profile.Has(ZombieTemplate::NewspaperSledDiggerGiga);
    const bool coneImpFootballGiantTemplate = profile.Has(ZombieTemplate::ConeImpFootballGiant);
    const bool normalNewsSledTemplate = profile.Has(ZombieTemplate::NormalNewsSled);
    const bool normalNewsImpSundayTemplate = profile.Has(ZombieTemplate::NormalNewsImpSunday);
    const bool ladderPoleTemplate = profile.Has(ZombieTemplate::LadderPole);
    const bool newspaperFanPoleTemplate = profile.Has(ZombieTemplate::NewspaperFanPole);
    const bool peaHeadSundayTemplate = profile.Has(ZombieTemplate::PeaHeadSunday);
    const bool peaHeadZamboniTemplate = profile.Has(ZombieTemplate::PeaHeadZamboni);
    const bool peaHeadFlagBungeeTemplate = profile.Has(ZombieTemplate::PeaHeadFlagBungee);
    const bool moundSkirmishTemplate = profile.Has(ZombieTemplate::MoundSkirmish);
    const bool flagSquashTemplate = profile.Has(ZombieTemplate::FlagSquash);
    const bool fanImpTemplate = profile.Has(ZombieTemplate::FanImp);
    const bool moundTallnutSledTemplate = profile.Has(ZombieTemplate::MoundTallnutSled);
    const bool impLadderFootballTemplate = profile.Has(ZombieTemplate::ImpLadderFootball);
    const bool sledDogHeavyTemplate = profile.Has(ZombieTemplate::SledDogHeavy);
    const bool dogSledPeaTemplate = profile.Has(ZombieTemplate::DogSledPea);
    // These winning replay decks were previously represented only by
    // generic card scores. Keep their observed release order explicit while
    // leaving target, grave-screen and anti-Ash constraints in charge.
    const bool ladderBalloonZamboniTemplate = profile.Has(ZombieTemplate::LadderBalloonZamboni);
    const bool moundBungeeFootballTemplate = profile.Has(ZombieTemplate::MoundBungeeFootball);
    const bool newspaperImpFootballGiantTemplate = profile.Has(ZombieTemplate::NewspaperImpFootballGiant);
    const bool peaHeadZomblobGiantTemplate = profile.Has(ZombieTemplate::PeaHeadZomblobGiant);
    const bool impPailSledFootballTemplate = profile.Has(ZombieTemplate::ImpPailSledFootball);
    const int peaHeadCount = static_cast<int>(std::count_if(state.zombies.begin(), state.zombies.end(), [](const VSZombieState &zombie) {
        return !zombie.dead && zombie.zombieType == static_cast<std::uint16_t>(ZombieType::ZOMBIE_PEA_HEAD);
    }));
    const bool earlyRangedSiegeDeployment = rangedSiegeTemplate && economyCount >= 2 && economyCount <= state.rows + 1
        && context.activePressureRows < std::min(3, state.rows);
    const bool openingPeaHeadPhase = peaHeadGiantTemplate && economyCount >= 2 && economyCount <= state.rows + 2
        && peaHeadCount < std::min(3, state.rows);
    // The Pea-head/Flag replay opens with distant ranged pressure on several
    // rows. Flag is a later coordinated release, not the first cheap body;
    // Bungee remains reserved for a legal high-value target by its separate
    // target filter.
    const bool peaHeadFlagRelease = peaHeadFlagBungeeTemplate && economyCount >= state.rows * 2
        && peaHeadCount >= std::min(3, state.rows) && context.activePressureRows >= 2
        && areaCounterExposure < 120;
    const bool replayOpeningSpread = economyCount >= 2 && economyCount <= state.rows + 1
        && context.activePressureRows < std::min(3, state.rows);
    const int templatePhaseBonus = ZombieTemplatePhaseBonus(profile, tempo, seed, context.actualEconomyCount,
        context.activePressureRows, zombieCount, state.rows);
    const ZombieTemplateTacticalState templateState{
        .economyCount = economyCount,
        .activePressureRows = context.activePressureRows,
        .zombiesInRow = zombieCount,
        .rows = state.rows,
        .peaHeadCount = peaHeadCount,
        .plantCount = plantCount,
        .economyValue = economyValue,
        .sustainedOutput = sustainedOutput,
        .areaCounterExposure = areaCounterExposure,
        .hasWallnut = hasWallnut,
        .graveUnderDirectFire = graveProjectileThreat >= 55,
    };
    const int templateTacticalBonus = ZombieTemplateTacticalBonus(profile, seed, templateState);

    int score = 20 + ZombieLaneAttackScore(state, targetRow);
    const int graveThreat = ProtectableGraveThreatScore(state, targetRow);
    // A grave is the zombie player's income source. Any available
    // pressure is deliberately biased toward a lane that is shooting it.
    score += graveThreat * 2;
    if (IsZombieFastAttackSeed(seed) && economyCount >= 1 && economyCount <= state.rows + 2) {
        // The quick-attack recordings use low-cost bodies to make the
        // plant player defend several income rows before heavy cards are
        // affordable. Reward an unoccupied economy lane, not a pileup.
        score += 145 + economyValue / 2;
        score += zombieCount == 0 ? 110 : -90;
    }
    switch (seed) {
        case SeedType::SEED_ZOMBIE_BOBSLED:
            // After the opening graves, the replay's first proactive pressure is Bobsled into a held lane.
            score += 95 + plantCount * 16 + sustainedOutput / 2 + economyValue / 3 + (hasSnowPea ? 190 : 0) + (hasBonkChoy ? 120 : 0);
            // A sled team is already an area-counter magnet. Keep its first
            // commitment on an empty route and use it to open a second lane,
            // instead of stacking four riders into one Cherry/Squash cell.
            score += zombieCount == 0 ? 300 : -420;
            // The Imp/Pail/Sled/Sunday recording holds its Sled while Imps
            // and Pails establish the early routes, then uses Sunday Edition
            // as the first real conversion. Keep Sled as a later independent
            // wave instead of treating it as the mandatory three-grave play.
            score += impSledSundayTemplate
                ? (economyCount < std::max(state.rows + 3, 8) || context.activePressureRows < 3
                    ? -240
                    : (zombieCount == 0 && areaCounterExposure < 135 ? -40 : -190))
                : 0;
            // The Normal/Newspaper/Sled recording uses Bobsled as an
            // isolated second wave after cheap probes have spread out.
            score += normalNewsSledTemplate && economyCount >= 3 && context.activePressureRows >= 2
                ? (zombieCount == 0 ? 175 : -210)
                : 0;
            score += newspaperSledDiggerGigaTemplate && economyCount >= state.rows
                    && context.activePressureRows >= 2
                ? (zombieCount == 0 ? 145 : -190)
                : 0;
            score += sledDogHeavyTemplate && economyCount >= state.rows && context.activePressureRows >= 2
                ? (zombieCount == 0 ? 135 : -180)
                : 0;
            score += moundTallnutSledTemplate && economyCount >= state.rows && hasWallnut
                ? (zombieCount == 0 ? 115 : -160)
                : 0;
            // Dog/Sled/Pea gains tempo by making several cheap lanes answer
            // first. Sled is the later independent route, never the first
            // body delivered into an available Ash counter.
            if (dogSledPeaTemplate) {
                score += economyCount >= state.rows && peaHeadCount >= 2
                    && context.activePressureRows >= 2 && areaCounterExposure < 135
                    ? (zombieCount == 0 ? 170 : -230)
                    : -120;
            }
            // Pea Head/Zomblob uses the sled as a later independent lane
            // opener, once its rear firing pressure has forced responses.
            score += peaHeadZomblobGiantTemplate && economyCount >= state.rows
                    && peaHeadCount >= 2 && zombieCount == 0
                ? 150
                : 0;
            break;
        case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
            score += 80 + plantCount * 12 + sustainedOutput / 3 + (hasSnowPea ? 115 : 0) + (hasWallnut ? 80 : 0);
            break;
        case SeedType::SEED_ZOMBIE_TALLNUT_HEAD:
            // Mound games protect the upgraded economic asset with a
            // durable head rather than treating Trashcan as the only
            // viable grave screen.
            score += graveProjectileThreat > 0 && !hasGraveGuard ? 440 + graveProjectileThreat * 2 : -95;
            score += graveThreat >= 100 ? 115 : 0;
            break;
        case SeedType::SEED_ZOMBIE_PAIL:
            score += 65 + plantCount * 14 + sustainedOutput / 2 + economyValue / 4 + (hasSnowPea ? 135 : 0) + (hasBonkChoy ? 100 : 0);
            score += plantHasPeaCarry ? 70 : 0;
            // Magnet is a card-level matchup signal even before the magnet
            // is planted. Prefer a non-metal guard when the opponent has
            // committed that answer package.
            score -= plantHasMagnet ? 190 : 0;
            // A Pea Head opening first establishes ranged pressure in empty
            // lanes. Keep the first pail for a real screen decision rather
            // than letting it displace that formation by raw durability.
            score -= earlyRangedSiegeDeployment ? 115 : 0;
            score -= openingPeaHeadPhase ? 360 : 0;
            score += impSledSundayTemplate && economyCount >= 3 && economyCount <= state.rows * 2
                && zombieCount == 0 ? 90 : 0;
            // Imp/Pail/Sled/Football establishes two graves, then fans out
            // its cheap pair before converting an already live route with
            // Football. It never needs to rush the unused Sled first.
            score += impPailSledFootballTemplate && replayOpeningSpread
                ? (zombieCount == 0 ? 150 : -160)
                : 0;
            // In cone/imp/football games the first pails are a measured
            // mid-game reinforcement. Cones and Imps establish the broad
            // pressure first, except where a grave needs a screen now.
            score += coneImpFootballGiantTemplate && economyCount < 3
                && !(graveProjectileThreat > 0 && !hasGraveGuard) ? -210 : 0;
            score += peaHeadFlagBungeeTemplate && peaHeadCount < std::min(3, state.rows) ? -260 : 0;
            break;
        case SeedType::SEED_ZOMBONI:
            // The ice trail makes Zomboni a strong answer to protected,
            // developed lanes, matching the second replay's breakthrough.
            score += 115 + plantCount * 18 + sustainedOutput / 2 + economyValue / 3
                + ((hasWallnut || plantHasNutCard) ? 135 : 0) + (hasSnowPea ? 90 : 0);
            score += zamboniPoleTemplate && economyCount >= 3 && economyCount <= state.rows + 3
                ? (zombieCount == 0 ? 155 : -135)
                : 0;
            score += (normalNewsSledTemplate || peaHeadZamboniTemplate || moundSkirmishTemplate)
                    && economyCount >= 3 && (hasWallnut || sustainedOutput >= 75 || plantCount >= 3)
                ? (zombieCount == 0 ? 155 : -185)
                : 0;
            // Balloon/Cone/Ladder opens three distinct routes, then Zomboni
            // converts the first formed nut or firing lane. It is not sent
            // into an already occupied lane just to reproduce the replay.
            score += ladderBalloonZamboniTemplate && economyCount >= 2
                    && (hasWallnut || plantHasNutCard || sustainedOutput >= 65) && zombieCount == 0
                ? 175
                : 0;
            break;
        case SeedType::SEED_ZOMBIE_TRASHCAN:
            // Trashcan is deliberately a slow front-line shield: a
            // single one in the lane blocks pea-family fire before it
            // reaches the graves behind it.
            score += graveProjectileThreat > 0 && !hasGraveGuard ? 425 + graveProjectileThreat * 2 : -250;
            score -= lobbedProjectileThreat * 2;
            score += graveScreenDeficit * 2;
            score += graveThreat >= 100 ? 90 : 0;
            score -= plantHasMagnet ? 240 : 0;
            score -= plantHasShortPult ? 80 : 0;
            // A pult carry attacks over this slow screen. Penalize the
            // card-level matchup too, before the first pult is planted.
            score -= plantHasLobbedCard ? 380 : 0;
            // In the Pea Head/Giant template Trashcan comes after the
            // opening firing lanes, except when it is immediately required
            // to save an exposed grave from direct-fire damage.
            score -= openingPeaHeadPhase && !(graveProjectileThreat > 0 && !hasGraveGuard) ? 340 : 0;
            break;
        case SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER:
            // Giga Polevaulter needs a substantial economy, but it may
            // still be the mid-game release card once two lanes are live.
            {
                const bool hasBreakthroughTarget = plantCount >= 3 || hasWallnut || hasPumpkinShell || sustainedOutput >= 80 || economyValue >= 120;
                const bool earlyHeavyCommit = economyCount >= std::max(state.rows * 2, state.rows + 3)
                    && context.activePressureRows >= 2 && CountLivePlants(state) >= state.rows && areaCounterExposure < 120;
                // This exact ladder/pole recording releases a Giga Pole
                // notably earlier than the generic finisher plan, but only
                // after two low-cost lanes are live and a nut line gives it
                // a meaningful jump target.
                const bool replayPoleRelease = ladderPoleTemplate && economyCount >= 2
                    && context.activePressureRows >= 1 && (plantCount >= 1 || economyValue >= 50)
                    && areaCounterExposure < 120;
                const bool replayFanPoleRelease = newspaperFanPoleTemplate && economyCount >= 3
                    && context.activePressureRows >= 2 && CountLivePlants(state) >= state.rows
                    && areaCounterExposure < 120;
                score += (economyCount >= heavyEconomyThreshold || earlyHeavyCommit || replayPoleRelease || replayFanPoleRelease)
                    ? ((hasBreakthroughTarget || earlyHeavyCommit || replayPoleRelease || replayFanPoleRelease) ? 250 : 15) : -240;
                score += earlyHeavyCommit ? 110 : 0;
                score += replayPoleRelease ? 235 : 0;
                score += replayFanPoleRelease ? 180 : 0;
                score += plantCount * 16 + sustainedOutput / 2 + economyValue / 3;
                score += (hasWallnut ? 145 : 0) + (hasPumpkinShell ? 105 : 0) + (hasSnowPea ? 70 : 0);
                score += targetLane.defense >= 120 ? 75 : 0;
                score -= areaCounterExposure / 3;
                score -= zombieCount >= 2 ? 145 : 0;
            }
            break;
        case SeedType::SEED_ZOMBIE_GARGANTUAR:
        case SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR:
            // Heavy cards are release cards, not automatic reinforcements.
            // A human-like commit seeks a defended economic line to force
            // several answers, and avoids walking a giant into a formed
            // Ash cluster merely because friendly zombies are already there.
            {
                const bool hasBreakthroughTarget = plantCount >= 3 || hasWallnut || sustainedOutput >= 100 || economyValue >= 150;
                const bool hasBoardInvestment = CountLivePlants(state) >= state.rows;
                const int earlyEconomyFloor = seed == SeedType::SEED_ZOMBIE_GARGANTUAR
                    ? state.rows
                    : std::max(state.rows * 2, state.rows + 3);
                const bool earlyHeavyCommit = economyCount >= earlyEconomyFloor && context.activePressureRows >= 2
                    && hasBoardInvestment && areaCounterExposure < 120;
                // The Normal/Newspaper/Digger recording banks into a Giga
                // Gargantuar at roughly ten to eleven graves, then resumes
                // low-cost pressure. Keep that earlier conversion tied to
                // two active probes and an invested plant board.
                const bool replayGigaRelease = newspaperDiggerGigaTemplate
                    && seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR
                    && economyCount >= std::max(state.rows * 2, 9)
                    && context.activePressureRows >= 2 && hasBoardInvestment
                    && areaCounterExposure < 120;
                const bool replayFlagGigaRelease = flagSquashTemplate
                    && seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR && economyCount >= 8
                    && context.activePressureRows >= 2 && hasBoardInvestment
                    && areaCounterExposure < 120;
                const bool replayArmoredGigaRelease = armoredNormalRushTemplate
                    && seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR && economyCount >= 8
                    && context.activePressureRows >= 2 && hasBoardInvestment
                    && areaCounterExposure < 120;
                const bool replayImpFootballRelease = newspaperImpFootballGiantTemplate
                    && seed == SeedType::SEED_ZOMBIE_GARGANTUAR && economyCount >= state.rows + 2
                    && context.activePressureRows >= 2 && hasBoardInvestment
                    && areaCounterExposure < 120;
                const bool replayZomblobGigaRelease = peaHeadZomblobGiantTemplate
                    && seed == SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR && economyCount >= 8
                    && peaHeadCount >= 2 && context.activePressureRows >= 2 && hasBoardInvestment
                    && areaCounterExposure < 120;
                // Standard Gargantuars can convert a five-grave opening
                // into pressure; the more expensive variants wait for a
                // broader rear field. Both still need a real board state
                // to commit into instead of becoming an opening all-in.
                const bool hasMidGameHeavyEconomy = economyCount >= std::max(state.rows * 2, heavyEconomyThreshold - 2)
                    && hasBreakthroughTarget;
                const bool canCommitHeavy = economyCount >= heavyEconomyThreshold || hasMidGameHeavyEconomy
                    || earlyHeavyCommit || replayGigaRelease || replayFlagGigaRelease || replayArmoredGigaRelease
                    || replayImpFootballRelease || replayZomblobGigaRelease;
                score += canCommitHeavy
                    ? ((hasBreakthroughTarget || earlyHeavyCommit || replayGigaRelease || replayFlagGigaRelease || replayArmoredGigaRelease
                        || replayImpFootballRelease || replayZomblobGigaRelease) ? 285 : 35)
                    : -220;
                score += earlyHeavyCommit ? 125 : 0;
                score += replayGigaRelease ? 165 : 0;
                score += replayFlagGigaRelease ? 155 : 0;
                score += replayArmoredGigaRelease ? 165 : 0;
                score += replayImpFootballRelease ? 145 : 0;
                score += replayZomblobGigaRelease ? 165 : 0;
                score += plantCount * 18 + sustainedOutput / 2 + economyValue / 3;
                score += (hasWallnut ? 135 : 0) + (hasPumpkinShell ? 110 : 0) + (hasBonkChoy ? 100 : 0) + (hasSnowPea ? 75 : 0);
                score += targetLane.defense >= 150 ? 90 : 0;
                score -= areaCounterExposure / 2;
                score -= zombieCount >= 2 ? 125 : 0;
            }
            break;
        case SeedType::SEED_ZOMBIE_PEA_HEAD:
            // Pea Head is the persistent half of the ranged-siege replay.
            // Start it on a developed plant route, then let a separate
            // grave-guard decision preserve the rear economy. It should not
            // shadow a cheap probe merely because that probe reached a row
            // first.
            score += plantCount * 10 + sustainedOutput / 3 + economyValue / 4 + (hasSnowPea ? 120 : 0);
            score += rangedSiegeTemplate ? 145 + PlantEconomyValueInRow(state, targetRow) : 0;
            score += zombieCount == 0 ? 85 : -110;
            score += earlyRangedSiegeDeployment && zombieCount == 0 ? 190 : 0;
            score += openingPeaHeadPhase && zombieCount == 0 ? 230 : 0;
            score += peaHeadSundayTemplate && economyCount >= 2 && economyCount <= state.rows + 2
                && zombieCount == 0 ? 175 : 0;
            score += peaHeadZamboniTemplate && economyCount >= 2 && economyCount <= state.rows + 2
                && zombieCount == 0 ? 150 : 0;
            score += peaHeadFlagBungeeTemplate && economyCount >= 2 && economyCount <= state.rows + 3
                && peaHeadCount < std::min(3, state.rows) && zombieCount == 0 ? 185 : 0;
            score += dogSledPeaTemplate && economyCount >= 3 && economyCount <= state.rows + 2
                && peaHeadCount < std::min(3, state.rows) && zombieCount == 0 ? 185 : 0;
            break;
        case SeedType::SEED_ZOMBIE_NEWSPAPER:
        case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
            score += plantCount * 10 + sustainedOutput / 3 + economyValue / 4 + (hasSnowPea ? 120 : 0);
            score -= hasLobbedPlant ? 900 : 0;
            score -= plantHasLobbedCard ? 380 : 0;
            score -= plantHasMagnet ? 165 : 0;
            // Screen Door is a direct-fire grave screen in the Sled/Sunday
            // deck, not a generic pult counter. A protected grave earns the
            // tempo only after the initial Imp/Sled routes are alive.
            if (seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR && impSledSundayTemplate) {
                score += !hasLobbedPlant && graveProjectileThreat >= 70 && !hasGraveGuard ? 210 : -80;
            }
            if (seed == SeedType::SEED_ZOMBIE_NEWSPAPER && (ladderPoleTemplate || newspaperFanPoleTemplate
                || newspaperSledDiggerGigaTemplate) && economyCount <= state.rows + 2) {
                score += zombieCount == 0 ? 135 : -120;
            }
            // The Normal/Dog/Newspaper/Imp/Sunday recording uses its first
            // Newspaper as a two-grave pressure probe, then spends faster
            // cards on separate rows. A direct-fire screen is still
            // rejected above, so this only refines a legal first probe.
            if (seed == SeedType::SEED_ZOMBIE_NEWSPAPER && normalNewsImpSundayTemplate
                && economyCount >= 2 && economyCount <= state.rows && context.activePressureRows < std::min(3, state.rows)) {
                score += zombieCount == 0 ? 185 : -175;
            }
            // Cone/Newspaper/Imp/Football/Gargantuar opens by fanning a
            // paper screen and an Imp across separate grave routes. The
            // later Football and Gargantuar are release cards, not an
            // incentive to pile screens into the same lane.
            if (seed == SeedType::SEED_ZOMBIE_NEWSPAPER && newspaperImpFootballGiantTemplate && replayOpeningSpread) {
                score += zombieCount == 0 ? 175 : -165;
            }
            break;
        case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
            score += 45 + plantCount * 10 + sustainedOutput / 4 + economyValue / 5;
            score += hasSnowPea ? 70 : 0;
            score += zamboniPoleTemplate && economyCount <= state.rows + 2 ? (zombieCount == 0 ? 95 : -100) : 0;
            score += (coneImpFootballGiantTemplate || ladderPoleTemplate || flagSquashTemplate || fanImpTemplate)
                && economyCount <= state.rows + 2 ? (zombieCount == 0 ? 115 : -120) : 0;
            // After the opening Pea-head spread, a single Cone makes a
            // separate economy lane answerable without donating a Pail into
            // a Magnet-shroom package.
            score += peaHeadFlagBungeeTemplate && economyCount >= 3 && economyCount <= state.rows * 2
                && peaHeadCount >= 2 ? (zombieCount == 0 ? 130 : -165) : 0;
            score += ladderBalloonZamboniTemplate && replayOpeningSpread
                ? (zombieCount == 0 ? 135 : -135)
                : 0;
            score += newspaperImpFootballGiantTemplate && replayOpeningSpread
                ? (zombieCount == 0 ? 120 : -140)
                : 0;
            break;
        case SeedType::SEED_ZOMBIE_LADDER:
            // Ladders are only a worthwhile commitment against an
            // established nut line; otherwise a cheaper probe is better.
            score += hasWallnut ? 275 : (plantHasNutCard ? 220 : -65);
            score += plantCount * 8 + sustainedOutput / 3 + economyValue / 4;
            score += ladderBalloonZamboniTemplate && replayOpeningSpread && (hasWallnut || plantHasNutCard)
                ? 125
                : 0;
            break;
        case SeedType::SEED_ZOMBIE_SUNDAY_EDITION:
            // Sunday Edition is a release card, but the fast-pressure
            // recordings do not wait for a full fifteen-grave bank. Once
            // several cheap routes are already taxing the plant player it
            // converts a four-grave opening into a real second-wave threat.
            // Without that multi-lane setup it still waits for the mature
            // economy so it is not an isolated expensive donation.
            {
                const bool earlySundayRelease = sundayPressureTemplate && !impSledSundayTemplate
                    && economyCount >= std::max(3, state.rows - 1) && context.activePressureRows >= 2
                    && areaCounterExposure < 150;
                const bool sledSundayRelease = impSledSundayTemplate
                    && economyCount >= std::max(state.rows + 3, 8) && context.activePressureRows >= 2
                    && areaCounterExposure < 145;
                const bool peaHeadSundayRelease = peaHeadSundayTemplate
                    && economyCount >= state.rows + 2 && peaHeadCount >= 2
                    && context.activePressureRows >= 2 && areaCounterExposure < 145;
                const bool canReleaseSunday = economyCount >= heavyEconomyThreshold || earlySundayRelease
                    || sledSundayRelease || peaHeadSundayRelease;
                score += canReleaseSunday ? 145 : -170;
                score += earlySundayRelease ? 155 : 0;
                score += sledSundayRelease ? 190 : 0;
                score += peaHeadSundayRelease ? 205 : 0;
            }
            score += plantCount * 14 + sustainedOutput / 2 + economyValue / 3;
            score += targetLane.defense >= 120 ? 70 : 0;
            score -= areaCounterExposure / 3;
            break;
        case SeedType::SEED_ZOMBIE_IMP:
        case SeedType::SEED_ZOMBIE_DIGGER:
            score += plantCount * 8 + sustainedOutput / 4 + economyValue / 2 + (hasWallnut ? 90 : 0);
            score += seed == SeedType::SEED_ZOMBIE_IMP && zamboniPoleTemplate && economyCount <= state.rows + 2
                ? (zombieCount == 0 ? 145 : -115)
                : 0;
            score += seed == SeedType::SEED_ZOMBIE_IMP && impSledSundayTemplate
                    && economyCount >= 3 && economyCount <= state.rows * 2
                    ? (zombieCount == 0 ? 155 : -135)
                    : 0;
            score += seed == SeedType::SEED_ZOMBIE_IMP && (coneImpFootballGiantTemplate || peaHeadSundayTemplate
                || moundSkirmishTemplate || impLadderFootballTemplate) && economyCount <= state.rows + 2
                    ? (zombieCount == 0 ? 130 : -145)
                    : 0;
            score += seed == SeedType::SEED_ZOMBIE_IMP && normalNewsImpSundayTemplate
                && economyCount >= 3 && economyCount <= state.rows + 2
                && context.activePressureRows < std::min(3, state.rows)
                ? (zombieCount == 0 ? 165 : -175)
                : 0;
            score += seed == SeedType::SEED_ZOMBIE_IMP && newspaperImpFootballGiantTemplate && replayOpeningSpread
                ? (zombieCount == 0 ? 170 : -170)
                : 0;
            score += seed == SeedType::SEED_ZOMBIE_IMP && impPailSledFootballTemplate && replayOpeningSpread
                ? (zombieCount == 0 ? 180 : -180)
                : 0;
            score += seed == SeedType::SEED_ZOMBIE_IMP && dogSledPeaTemplate
                    && economyCount >= 3 && economyCount <= state.rows + 1
                    && context.activePressureRows < std::min(3, state.rows)
                ? (zombieCount == 0 ? 155 : -165)
                : 0;
            // Digger is a rear-economic strike, never an opening body.
            // Require a developed producer/carry row before spending it.
            score += seed == SeedType::SEED_ZOMBIE_DIGGER && newspaperDiggerGigaTemplate
                ? (economyCount >= state.rows && (economyValue >= 110 || sustainedOutput >= 85)
                    ? 245 : -320)
                : 0;
            break;
        case SeedType::SEED_ZOMBIE_FOOTBALL:
        case SeedType::SEED_ZOMBIE_GIGA_FOOTBALL:
            // Football is the mid-game runner behind a Pea Head firing
            // spread. Giga Football additionally tackles its first plant,
            // but its strategic deployment remains the same as Football.
            // Before that spread exists, keep the brains for graves and a
            // second ranged lane instead of donating an isolated rush.
            score += plantCount * 15 + sustainedOutput / 2 + economyValue / 3 + (hasSnowPea ? 95 : 0);
            score += peaHeadGiantTemplate && peaHeadCount >= 2 && economyCount >= state.rows + 2 ? 180 : -110;
            score += zombieCount == 0 ? 80 : -170;
            score += (coneImpFootballGiantTemplate || impLadderFootballTemplate)
                    && economyCount >= state.rows && context.activePressureRows >= 2
                    && (hasWallnut || sustainedOutput >= 75)
                ? (zombieCount == 0 ? 155 : -140)
                : 0;
            score += newspaperImpFootballGiantTemplate && economyCount >= state.rows
                    && context.activePressureRows >= 2 && (hasWallnut || sustainedOutput >= 65)
                ? (zombieCount == 0 ? 165 : -150)
                : 0;
            score += impPailSledFootballTemplate && economyCount >= 2 && context.activePressureRows >= 2
                ? (zombieCount == 0 ? 175 : -170)
                : 0;
            score += seed == SeedType::SEED_ZOMBIE_GIGA_FOOTBALL && moundBungeeFootballTemplate
                    && economyCount >= state.rows && context.activePressureRows >= 2
                ? (zombieCount == 0 ? 165 : -150)
                : 0;
            score -= areaCounterExposure / 3;
            break;
        case SeedType::SEED_ZOMBIE_NORMAL:
        case SeedType::SEED_ZOMBIE_DOGWALKER:
        case SeedType::SEED_ZOMBIE_FLAG:
            // Normal/Dog/Flag recordings win tempo by touching several
            // Sunflower lanes while graves are still being built. Treat
            // these as a network of cheap probes, never as a second body
            // behind an already answered zombie.
            score += 120 + economyValue * 2 / 3 + plantCount * 7;
            score += fastPressureTemplate ? 120 : 0;
            score += zombieCount == 0 ? 145 : -220;
            score += CountZombiesInRow(state, targetRow) == 0 && EconomyPlantsInRow(state, targetRow) > 0 ? 85 : 0;
            score += (newspaperDiggerGigaTemplate || normalNewsSledTemplate || moundSkirmishTemplate || flagSquashTemplate)
                && economyCount <= state.rows + 2 && zombieCount == 0 ? 110 : 0;
            // The armored normal deck uses exactly one early Normal per
            // exposed route. It then returns to grave construction until
            // the protected multi-lane board can fund its Giga release.
            score += armoredNormalRushTemplate && economyCount >= 1 && economyCount <= state.rows + 2
                ? (zombieCount == 0 ? 135 : -180)
                : 0;
            score += normalNewsImpSundayTemplate && economyCount >= 3 && economyCount <= state.rows + 2
                    && context.activePressureRows < std::min(3, state.rows)
                ? (zombieCount == 0 ? 145 : -185)
                : 0;
            score += seed == SeedType::SEED_ZOMBIE_DOGWALKER && dogSledPeaTemplate
                    && economyCount >= 2 && economyCount <= state.rows + 2
                    && context.activePressureRows < std::min(3, state.rows)
                ? (zombieCount == 0 ? 175 : -200)
                : 0;
            score += seed == SeedType::SEED_ZOMBIE_FLAG && peaHeadFlagBungeeTemplate
                ? (peaHeadFlagRelease ? (zombieCount == 0 ? 235 : 80) : -430)
                : 0;
            break;
        case SeedType::SEED_ZOMBIE_SUPER_FAN_IMP:
            // Cheap fast pressure should fan out through under-defended
            // sunflower lanes, not shadow an existing zombie stack.
            score += 105 + plantCount * 9 + sustainedOutput / 3 + economyValue / 2;
            score += zombieCount == 0 ? 110 : -90;
            score += fanImpTemplate && economyCount <= state.rows + 2 && zombieCount == 0 ? 125 : 0;
            score += newspaperFanPoleTemplate && economyCount <= state.rows + 2 && zombieCount == 0 ? 125 : 0;
            break;
        case SeedType::SEED_ZOMBIE_SQUASH_HEAD:
            score += 95 + plantCount * 11 + sustainedOutput / 3 + economyValue / 3;
            score += zombieCount == 0 ? 90 : -75;
            score += (flagSquashTemplate || fanImpTemplate) && economyCount >= 2
                && (economyValue >= 80 || plantCount >= 2) && zombieCount == 0 ? 130 : 0;
            break;
        case SeedType::SEED_ZOMBIE_BUNGEE:
            // Bungee needs a real high-value carry to steal. A deck-level
            // carry is enough to reserve the card, but never let it outrank
            // a useful frontline probe when the plant deck is only pads.
            score += hasPlants ? 220 : -80;
            score += plantHasHighValueCarryCard ? 260 : -180;
            score += (hasWallnut || hasBonkChoy) ? 85 : 0;
            // Mound/Bungee/Football holds Bungee until the upgraded grave
            // base has bought enough time for a real carry to appear.
            score += moundBungeeFootballTemplate && economyCount >= state.rows && plantHasHighValueCarryCard ? 145 : -120;
            break;
        case SeedType::SEED_ZOMBIE_GRAVESTONE:
            if (economyCount < economyTarget) {
                // Replay construction spans the full three zombie-side
                // columns. The fixed four-grave opening was too small.
                score += 450 + (economyTarget - economyCount) * 35;
            } else {
                score -= 180;
            }
            score += plantCount * 4;
            break;
        case SeedType::SEED_ZOMBIE_MOUND:
            // A mound is an upgrade to a grave economy, not a substitute
            // for the initial rear field.
            score += economyCount >= std::max(3, state.rows / 2) ? 220 : -220;
            score += graveThreat;
            score += (moundSkirmishTemplate || moundTallnutSledTemplate) && economyCount >= 3 ? 115 : 0;
            // The recorded Mound/Bungee line upgrades only after the second
            // rear grave. Per-mound affordability and protection still come
            // from FindZombieMoundCell and the outer decision layer.
            score += moundBungeeFootballTemplate && economyCount >= 2 && economyCount <= state.rows + 2
                && graveScreenDeficit < 80 && graveThreat < 80 ? 310 : 0;
            break;
        case SeedType::SEED_ZOMBIE_DANCER:
            score += 75 + plantCount * 12 + (graveThreat > 0 ? 35 : 0);
            break;
        case SeedType::SEED_ZOMBIE_CATAPULT:
            // Unlike a Door or Newspaper, the Catapult can pressure a
            // pult line without presenting its screen to the lobbed
            // projectiles. Commit it to a developed firing lane, unless
            // FindTarget rejects that lane for a hypnotized zombie.
            score += 155 + plantCount * 14 + sustainedOutput / 2 + economyValue / 3;
            score += hasLobbedPlant ? 145 : 0;
            score += (hasSnowPea ? 90 : 0) + (hasPumpkinShell ? 55 : 0);
            score -= areaCounterExposure / 4;
            break;
        case SeedType::SEED_ZOMBIE_BALLOON:
            score += 65 + plantCount * 8 + (hasSnowPea ? 75 : 0);
            score += ladderBalloonZamboniTemplate && replayOpeningSpread
                ? (zombieCount == 0 ? 185 : -170)
                : 0;
            break;
        case SeedType::SEED_ZOMBIE_ZOMBLOB:
            // Pea Head/Zomblob first spreads distant damage, then uses the
            // blob to convert a defended economy lane once multiple routes
            // are live. A single isolated blob is an expensive donation.
            score += plantCount * 12 + sustainedOutput / 2 + economyValue / 3;
            score += peaHeadZomblobGiantTemplate && economyCount >= 3 && peaHeadCount >= 2
                && context.activePressureRows >= 2
                ? (zombieCount == 0 ? 185 : -170)
                : 0;
            break;
        default:
            score += plantCount * 7 + sustainedOutput / 4 + economyValue / 4;
            break;
    }
    score += templatePhaseBonus;
    score += templateTacticalBonus;
    if (seed != SeedType::SEED_ZOMBIE_TRASHCAN && IsZombieGraveGuardSeed(seed)) {
        // The replay with Screen Door has no Trashcan.  A Door, Pail or
        // Wall-nut Head must still be allowed to screen direct fire from
        // the zombie-side economy instead of treating Trashcan as unique.
        score += graveProjectileThreat > 0 && !hasGraveGuard ? 260 + graveProjectileThreat : -35;
        score += lobbedProjectileThreat > 0 && !hasGraveGuard ? 180 + lobbedProjectileThreat : 0;
    }
    const bool isEconomyOrTargetedSeed = IsZombieEconomySeed(seed) || IsZombieTargetedSeed(seed);
    const bool isEmergencyGraveGuard = IsZombieGraveGuardSeed(seed) && graveProjectileThreat > 0 && !hasGraveGuard;
    if (zombieCount > 0 && !isEconomyOrTargetedSeed && !IsHeavyZombieSeed(seed) && !isEmergencyGraveGuard) {
        // A cheap/medium zombie is a probe, not a reason to feed the
        // same Ash target.  After one probe, opening another line with
        // Sunflowers is more valuable than reinforcing this line.
        score -= 340 + (zombieCount - 1) * 210;
        score -= areaCounterExposure;
        if (EconomyPlantsInRow(state, targetRow) == 0) {
            score -= 90;
        }
    }
    score += StrategyBonus(state, VSSide::Zombies, seed, targetRow);
    score -= effectiveCost / 50;
    return score;
}

} // namespace vsai::detail
