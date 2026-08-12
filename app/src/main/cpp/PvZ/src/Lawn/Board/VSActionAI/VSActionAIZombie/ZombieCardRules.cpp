#include "ZombieCardRules.h"

#include <algorithm>
#include <initializer_list>

namespace vsai::detail {

namespace {

constexpr std::uint32_t TemplateMask(ZombieTemplate value) {
    return 1U << static_cast<std::uint8_t>(value);
}

} // namespace

bool ZombieTemplateProfile::Has(ZombieTemplate value) const {
    return (templates & TemplateMask(value)) != 0;
}

ZombieTemplateProfile DetectZombieTemplateProfile(const VSGameState &state) {
    const auto has = [&state](SeedType seed) {
        return HasActiveDeckCard(state, VSSide::Zombies, seed);
    };
    const auto hasAll = [&has](std::initializer_list<SeedType> seeds) {
        for (const SeedType seed : seeds) {
            if (!has(seed)) {
                return false;
            }
        }
        return true;
    };
    const auto add = [](ZombieTemplateProfile &profile, ZombieTemplate value, bool matches) {
        if (matches) {
            profile.templates |= TemplateMask(value);
        }
    };

    ZombieTemplateProfile profile{};
    profile.fastPressure = (has(SeedType::SEED_ZOMBIE_NORMAL) || has(SeedType::SEED_ZOMBIE_DOGWALKER)
        || has(SeedType::SEED_ZOMBIE_SUPER_FAN_IMP) || has(SeedType::SEED_ZOMBIE_FLAG))
        && (has(SeedType::SEED_ZOMBIE_NEWSPAPER) || has(SeedType::SEED_ZOMBIE_IMP)
            || has(SeedType::SEED_ZOMBIE_TRAFFIC_CONE));
    profile.rangedSiege = has(SeedType::SEED_ZOMBIE_PEA_HEAD)
        && (has(SeedType::SEED_ZOMBIE_TRASHCAN) || has(SeedType::SEED_ZOMBIE_PAIL)
            || has(SeedType::SEED_ZOMBIE_FOOTBALL));
    profile.sundayPressure = has(SeedType::SEED_ZOMBIE_SUNDAY_EDITION)
        && (has(SeedType::SEED_ZOMBIE_NORMAL) || has(SeedType::SEED_ZOMBIE_IMP)
            || has(SeedType::SEED_ZOMBIE_NEWSPAPER));

    add(profile, ZombieTemplate::ZamboniPole, hasAll({SeedType::SEED_ZOMBONI, SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER,
        SeedType::SEED_ZOMBIE_PAIL, SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_IMP}));
    add(profile, ZombieTemplate::PeaHeadGiant, hasAll({SeedType::SEED_ZOMBIE_PEA_HEAD, SeedType::SEED_ZOMBIE_PAIL,
        SeedType::SEED_ZOMBIE_TRASHCAN}) && (has(SeedType::SEED_ZOMBIE_GARGANTUAR) || has(SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR)));
    add(profile, ZombieTemplate::ImpSledSunday, hasAll({SeedType::SEED_ZOMBIE_IMP, SeedType::SEED_ZOMBIE_PAIL,
        SeedType::SEED_ZOMBIE_BOBSLED, SeedType::SEED_ZOMBIE_SUNDAY_EDITION, SeedType::SEED_ZOMBIE_SCREEN_DOOR}));
    add(profile, ZombieTemplate::ArmoredNormalRush, hasAll({SeedType::SEED_ZOMBIE_NORMAL, SeedType::SEED_ZOMBIE_TRASHCAN,
        SeedType::SEED_ZOMBIE_DOGWALKER, SeedType::SEED_ZOMBIE_FOOTBALL})
        && (has(SeedType::SEED_ZOMBIE_GARGANTUAR) || has(SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR)));
    add(profile, ZombieTemplate::NewspaperDiggerGiga, hasAll({SeedType::SEED_ZOMBIE_NORMAL, SeedType::SEED_ZOMBIE_NEWSPAPER,
        SeedType::SEED_ZOMBIE_DIGGER, SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR}));
    add(profile, ZombieTemplate::NewspaperSledDiggerGiga, profile.Has(ZombieTemplate::NewspaperDiggerGiga)
        && has(SeedType::SEED_ZOMBIE_BOBSLED));
    add(profile, ZombieTemplate::ConeImpFootballGiant, hasAll({SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_PAIL,
        SeedType::SEED_ZOMBIE_IMP, SeedType::SEED_ZOMBIE_FOOTBALL, SeedType::SEED_ZOMBIE_GARGANTUAR}));
    add(profile, ZombieTemplate::NormalNewsSled, hasAll({SeedType::SEED_ZOMBIE_NORMAL, SeedType::SEED_ZOMBIE_NEWSPAPER,
        SeedType::SEED_ZOMBONI, SeedType::SEED_ZOMBIE_BOBSLED, SeedType::SEED_ZOMBIE_DOGWALKER}));
    add(profile, ZombieTemplate::NormalNewsImpSunday, hasAll({SeedType::SEED_ZOMBIE_NORMAL, SeedType::SEED_ZOMBIE_DOGWALKER,
        SeedType::SEED_ZOMBIE_NEWSPAPER, SeedType::SEED_ZOMBIE_IMP, SeedType::SEED_ZOMBIE_SUNDAY_EDITION}));
    add(profile, ZombieTemplate::LadderPole, hasAll({SeedType::SEED_ZOMBIE_NEWSPAPER, SeedType::SEED_ZOMBIE_TRAFFIC_CONE,
        SeedType::SEED_ZOMBIE_LADDER, SeedType::SEED_ZOMBIE_BOBSLED, SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER}));
    add(profile, ZombieTemplate::NewspaperFanPole, hasAll({SeedType::SEED_ZOMBIE_NORMAL, SeedType::SEED_ZOMBIE_NEWSPAPER,
        SeedType::SEED_ZOMBIE_SUPER_FAN_IMP, SeedType::SEED_ZOMBIE_GIGA_FOOTBALL, SeedType::SEED_ZOMBIE_GIGA_POLEVAULTER,
        SeedType::SEED_ZOMBIE_DOGWALKER}));
    add(profile, ZombieTemplate::PeaHeadSunday, hasAll({SeedType::SEED_ZOMBIE_PEA_HEAD, SeedType::SEED_ZOMBIE_IMP,
        SeedType::SEED_ZOMBIE_TRASHCAN, SeedType::SEED_ZOMBIE_SUNDAY_EDITION, SeedType::SEED_ZOMBIE_GARGANTUAR}));
    add(profile, ZombieTemplate::PeaHeadZamboni, hasAll({SeedType::SEED_ZOMBIE_PEA_HEAD, SeedType::SEED_ZOMBIE_PAIL,
        SeedType::SEED_ZOMBIE_TRASHCAN, SeedType::SEED_ZOMBONI, SeedType::SEED_ZOMBIE_GARGANTUAR}));
    add(profile, ZombieTemplate::PeaHeadFlagBungee, hasAll({SeedType::SEED_ZOMBIE_PEA_HEAD, SeedType::SEED_ZOMBIE_TRAFFIC_CONE,
        SeedType::SEED_ZOMBIE_PAIL, SeedType::SEED_ZOMBIE_FOOTBALL, SeedType::SEED_ZOMBIE_BUNGEE, SeedType::SEED_ZOMBIE_FLAG}));
    add(profile, ZombieTemplate::MoundSkirmish, hasAll({SeedType::SEED_ZOMBIE_MOUND, SeedType::SEED_ZOMBIE_NORMAL,
        SeedType::SEED_ZOMBIE_IMP, SeedType::SEED_ZOMBIE_NEWSPAPER, SeedType::SEED_ZOMBONI}));
    add(profile, ZombieTemplate::FlagSquash, hasAll({SeedType::SEED_ZOMBIE_FLAG, SeedType::SEED_ZOMBIE_SQUASH_HEAD,
        SeedType::SEED_ZOMBIE_SCREEN_DOOR, SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR}));
    add(profile, ZombieTemplate::FanImp, hasAll({SeedType::SEED_ZOMBIE_SUPER_FAN_IMP, SeedType::SEED_ZOMBIE_SQUASH_HEAD,
        SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_SCREEN_DOOR, SeedType::SEED_ZOMBIE_TRASHCAN}));
    add(profile, ZombieTemplate::MoundTallnutSled, hasAll({SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_PAIL,
        SeedType::SEED_ZOMBIE_BOBSLED, SeedType::SEED_ZOMBIE_TRASHCAN, SeedType::SEED_ZOMBIE_TALLNUT_HEAD, SeedType::SEED_ZOMBIE_MOUND}));
    add(profile, ZombieTemplate::ImpLadderFootball, hasAll({SeedType::SEED_ZOMBIE_IMP, SeedType::SEED_ZOMBIE_GARGANTUAR,
        SeedType::SEED_ZOMBIE_LADDER, SeedType::SEED_ZOMBIE_FOOTBALL, SeedType::SEED_ZOMBIE_SCREEN_DOOR}));
    add(profile, ZombieTemplate::SledDogHeavy, hasAll({SeedType::SEED_ZOMBIE_BOBSLED, SeedType::SEED_ZOMBIE_DOGWALKER,
        SeedType::SEED_ZOMBIE_PAIL, SeedType::SEED_ZOMBIE_GARGANTUAR, SeedType::SEED_ZOMBIE_GIGA_FOOTBALL}));
    add(profile, ZombieTemplate::DogSledPea, hasAll({SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_BOBSLED,
        SeedType::SEED_ZOMBIE_DOGWALKER, SeedType::SEED_ZOMBIE_PEA_HEAD, SeedType::SEED_ZOMBIE_IMP}));
    add(profile, ZombieTemplate::LadderBalloonZamboni, hasAll({SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_LADDER,
        SeedType::SEED_ZOMBONI, SeedType::SEED_ZOMBIE_BALLOON, SeedType::SEED_ZOMBIE_TALLNUT_HEAD, SeedType::SEED_ZOMBIE_JALAPENO_HEAD}));
    add(profile, ZombieTemplate::MoundBungeeFootball, hasAll({SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_TRASHCAN,
        SeedType::SEED_ZOMBIE_MOUND, SeedType::SEED_ZOMBIE_GIGA_FOOTBALL, SeedType::SEED_ZOMBIE_BUNGEE}));
    add(profile, ZombieTemplate::NewspaperImpFootballGiant, hasAll({SeedType::SEED_ZOMBIE_TRAFFIC_CONE, SeedType::SEED_ZOMBIE_NEWSPAPER,
        SeedType::SEED_ZOMBIE_IMP, SeedType::SEED_ZOMBIE_FOOTBALL, SeedType::SEED_ZOMBIE_GARGANTUAR}));
    add(profile, ZombieTemplate::PeaHeadZomblobGiant, hasAll({SeedType::SEED_ZOMBIE_BOBSLED, SeedType::SEED_ZOMBIE_PEA_HEAD,
        SeedType::SEED_ZOMBIE_TRASHCAN, SeedType::SEED_ZOMBIE_ZOMBLOB, SeedType::SEED_ZOMBIE_GIGA_GARGANTUAR}));
    add(profile, ZombieTemplate::ImpPailSledFootball, hasAll({SeedType::SEED_ZOMBIE_IMP, SeedType::SEED_ZOMBIE_BOBSLED,
        SeedType::SEED_ZOMBIE_PAIL, SeedType::SEED_ZOMBIE_FOOTBALL}));
    return profile;
}

bool ZombieTempoPolicy::IsEnhanced() const {
    return mEnhanced;
}

int ZombieTempoPolicy::EffectiveEconomyCount(int actualCount) const {
    return mEnhanced ? EffectiveAIEconomyCount(VSSide::Zombies, actualCount) : actualCount;
}

int ZombieTempoPolicy::OpeningPressureRowTarget(int baseline, int rows) const {
    return mEnhanced ? std::min(rows, baseline + 1) : baseline;
}

int ZombieTempoPolicy::HeavyBankEconomyThreshold(int rows, int heavyEconomyThreshold) const {
    return std::max(rows + (mEnhanced ? 1 : 2), heavyEconomyThreshold - (mEnhanced ? 3 : 2));
}

ZombieTempoPolicy GetZombieTempoPolicy() {
    return ZombieTempoPolicy(vsai::IsEnhancedAIEnabled() && vsai::IsSideEnabled(VSSide::Zombies));
}

bool IsZombieTargetedSeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_BUNGEE;
}

bool IsZombieEconomySeed(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_GRAVESTONE || seed == SeedType::SEED_ZOMBIE_MOUND;
}

bool IsZombieFrontlineProbeSeed(SeedType seed) {
    if (IsZombieEconomySeed(seed) || IsZombieTargetedSeed(seed) || IsHeavyZombieSeed(seed)) {
        return false;
    }
    return seed != SeedType::SEED_ZOMBIE_TRASHCAN && seed != SeedType::SEED_ZOMBIE_WALLNUT_HEAD
        && seed != SeedType::SEED_ZOMBIE_TALLNUT_HEAD;
}

bool IsZombieFastAttackSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_NORMAL:
        case SeedType::SEED_ZOMBIE_IMP:
        case SeedType::SEED_ZOMBIE_SUPER_FAN_IMP:
        case SeedType::SEED_ZOMBIE_DOGWALKER:
        case SeedType::SEED_ZOMBIE_FLAG:
        case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
            return true;
        default:
            return false;
    }
}

bool IsZombieMetalGraveGuard(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_PAIL || seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR
        || seed == SeedType::SEED_ZOMBIE_TRASHCAN;
}

bool IsZombieLobbedScreenDonation(SeedType seed) {
    return seed == SeedType::SEED_ZOMBIE_TRASHCAN || seed == SeedType::SEED_ZOMBIE_SCREEN_DOOR
        || seed == SeedType::SEED_ZOMBIE_NEWSPAPER;
}

int ZombieGraveGuardPriority(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_TRASHCAN:
            return 520;
        case SeedType::SEED_ZOMBIE_TALLNUT_HEAD:
            return 465;
        case SeedType::SEED_ZOMBIE_WALLNUT_HEAD:
            return 410;
        case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
            return 380;
        case SeedType::SEED_ZOMBIE_PAIL:
            return 340;
        case SeedType::SEED_ZOMBIE_SUNDAY_EDITION:
            return 285;
        case SeedType::SEED_ZOMBIE_NEWSPAPER:
            return 255;
        case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
            return 160;
        default:
            return 0;
    }
}

} // namespace vsai::detail
