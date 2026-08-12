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
};

struct ZombieTemplateProfile {
    std::uint32_t templates = 0;
    bool fastPressure = false;
    bool rangedSiege = false;
    bool sundayPressure = false;

    bool Has(ZombieTemplate value) const;
};

ZombieTemplateProfile DetectZombieTemplateProfile(const VSGameState &state);

class ZombieTempoPolicy {
public:
    explicit ZombieTempoPolicy(bool enhanced) : mEnhanced(enhanced) {}

    bool IsEnhanced() const;
    int EffectiveEconomyCount(int actualCount) const;
    int OpeningPressureRowTarget(int baseline, int rows) const;
    int HeavyBankEconomyThreshold(int rows, int heavyEconomyThreshold) const;

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
