#ifndef PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_CARD_RULES_H
#define PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_CARD_RULES_H

#include "../VSActionAIStrategy.h"

#include <cstdint>

namespace vsai::detail {

enum class ZombieTemplate : std::uint8_t {
    ZamboniPole,
    PeaHeadGiant,
    ImpSledSunday,
    ArmoredNormalRush,
    NewspaperDiggerGiga,
    NewspaperSledDiggerGiga,
    ConeImpFootballGiant,
    NormalNewsSled,
    NormalNewsImpSunday,
    LadderPole,
    NewspaperFanPole,
    PeaHeadSunday,
    PeaHeadZamboni,
    PeaHeadFlagBungee,
    MoundSkirmish,
    FlagSquash,
    FanImp,
    MoundTallnutSled,
    ImpLadderFootball,
    SledDogHeavy,
    DogSledPea,
    LadderBalloonZamboni,
    MoundBungeeFootball,
    NewspaperImpFootballGiant,
    PeaHeadZomblobGiant,
    ImpPailSledFootball,
    NewspaperScreenFootball,
    DogPeaFootball,
    NewspaperFootballPole,
    DancerRaid,
    PeaHeadRaid,
    PeaHeadDancerRaid,
    MoundPeaZomblobFootball,
    SundayLadderRaid,
    MoundNewspaperZamboni,
};

enum class ZombieTemplatePhase : std::uint8_t {
    Opening,
    Conversion,
    Finisher,
};

struct ZombieTemplateProfile {
    std::uint64_t templates = 0;
    bool fastPressure = false;
    bool rangedSiege = false;
    bool sundayPressure = false;

    bool Has(ZombieTemplate value) const;
};

class ZombieTempoPolicy;

ZombieTemplateProfile DetectZombieTemplateProfile(const VSGameState &state);
bool IsZombieTemplatePhaseSeed(const ZombieTemplateProfile &profile, SeedType seed, ZombieTemplatePhase phase);
bool IsZombieTemplatePhaseAvailable(const ZombieTemplateProfile &profile, const ZombieTempoPolicy &tempo, SeedType seed,
    int actualEconomyCount, int activePressureRows, int rows, ZombieTemplatePhase phase);
bool HasReadyZombieTemplateCommit(const VSGameState &state, const ZombieTemplateProfile &profile,
    const ZombieTempoPolicy &tempo, int actualEconomyCount, int activePressureRows);
int ZombieTemplatePhaseBonus(const ZombieTemplateProfile &profile, const ZombieTempoPolicy &tempo, SeedType seed,
    int actualEconomyCount, int activePressureRows, int zombiesInRow, int rows);

class ZombieTempoPolicy {
public:
    explicit ZombieTempoPolicy(bool enhanced) : mEnhanced(enhanced) {}

    bool IsEnhanced() const;
    int EffectiveEconomyCount(int actualCount) const;
    int EconomyTarget(int baseline, int rows) const;
    int OpeningEconomyFloor(int baseline) const;
    int OpeningEconomyCeiling(int baseline) const;
    int OpeningPressureRowTarget(int baseline, int rows) const;
    int HeavyBankEconomyThreshold(int rows, int heavyEconomyThreshold) const;
    std::uint8_t LaneAttackCooldown(SeedType seed) const;

private:
    bool mEnhanced;
};

ZombieTempoPolicy GetZombieTempoPolicy();

bool IsZombieTargetedSeed(SeedType seed);
bool IsZombieEconomySeed(SeedType seed);
bool IsZombieFrontlineProbeSeed(SeedType seed);
bool IsZombieFastAttackSeed(SeedType seed);
bool IsZombieMetalGraveGuard(SeedType seed);
bool IsZombieLobbedScreenDonation(SeedType seed);
int ZombieGraveGuardPriority(SeedType seed);

} // namespace vsai::detail

#endif // PVZ_LAWN_BOARD_VS_ACTION_AI_ZOMBIE_CARD_RULES_H
