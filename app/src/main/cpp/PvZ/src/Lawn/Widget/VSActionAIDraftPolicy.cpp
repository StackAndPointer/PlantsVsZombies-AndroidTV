#include "VSActionAIDraftPolicy.h"

#include "PvZ/SexyAppFramework/Buffer.h"
#include "PvZ/SexyAppFramework/SexyAppBase.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace vsai::draft {

namespace {

struct BanRule {
    bool targetsZombies = false;
    SeedType seed = SeedType::SEED_NONE;
    int priority = 0;
};

constexpr std::array<unsigned char, 8> kBanDatabaseMagic = {'P', 'V', 'Z', 'V', 'B', 'A', 'N', '\0'};
constexpr std::uint16_t kBanDatabaseVersion = 1;
constexpr std::size_t kBanDatabaseHeaderSize = 12;
constexpr std::size_t kBanDatabaseRuleSize = 7;
constexpr std::uint32_t kBanDatabaseRetryIntervalTicks = 300;

std::uint16_t ReadBanU16(const std::vector<unsigned char> &data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset]) | (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

class BanDatabase {
    std::vector<BanRule> mRules;
    BanDatabaseLoadState mLoadState = BanDatabaseLoadState::Uninitialized;
    std::uint32_t mNextRetryTick = 0;

    bool ShouldRetryAt(std::uint32_t tick) const {
        return static_cast<std::int32_t>(tick - mNextRetryTick) >= 0;
    }

    void MarkUnavailable(std::uint32_t tick) {
        mLoadState = BanDatabaseLoadState::Unavailable;
        mNextRetryTick = tick + kBanDatabaseRetryIntervalTicks;
    }

    void Load(std::uint32_t tick) {
        if (mLoadState == BanDatabaseLoadState::Loaded || mLoadState == BanDatabaseLoadState::Invalid
            || (mLoadState == BanDatabaseLoadState::Unavailable && !ShouldRetryAt(tick))) {
            return;
        }
        if (Sexy::gSexyAppBase == nullptr) {
            MarkUnavailable(tick);
            return;
        }

        Sexy::Buffer buffer;
        if (!Sexy::gSexyAppBase->ReadBufferFromFile("addonFiles/data/vs_ai_ban_db.bin", &buffer, false)) {
            MarkUnavailable(tick);
            return;
        }

        const std::vector<unsigned char> &data = buffer.mData;
        if (data.size() < kBanDatabaseHeaderSize
            || !std::equal(kBanDatabaseMagic.begin(), kBanDatabaseMagic.end(), data.begin())
            || ReadBanU16(data, 8) != kBanDatabaseVersion) {
            mLoadState = BanDatabaseLoadState::Invalid;
            return;
        }

        const std::size_t ruleCount = ReadBanU16(data, 10);
        if (ruleCount > (data.size() - kBanDatabaseHeaderSize) / kBanDatabaseRuleSize
            || kBanDatabaseHeaderSize + ruleCount * kBanDatabaseRuleSize != data.size()) {
            mLoadState = BanDatabaseLoadState::Invalid;
            return;
        }

        mRules.clear();
        for (std::size_t index = 0; index < ruleCount; ++index) {
            const std::size_t offset = kBanDatabaseHeaderSize + index * kBanDatabaseRuleSize;
            const unsigned char sideCode = data[offset];
            const int averageOrder = data[offset + 3];
            const int samples = data[offset + 4];
            const int priority = ReadBanU16(data, offset + 5);
            if (sideCode > 1 || averageOrder > 15 || samples <= 0 || priority <= 0 || priority > 1000) {
                continue;
            }
            mRules.push_back({sideCode == 1, static_cast<SeedType>(ReadBanU16(data, offset + 1)), priority});
        }
        mLoadState = BanDatabaseLoadState::Loaded;
    }

public:
    int Priority(bool targetsZombies, SeedType seed, std::uint32_t tick) {
        Load(tick);
        for (const BanRule &rule : mRules) {
            if (rule.targetsZombies == targetsZombies && rule.seed == seed) {
                return rule.priority;
            }
        }
        return 0;
    }

    BanDatabaseLoadState LoadState() const {
        return mLoadState;
    }

    void Reset() {
        mRules.clear();
        mLoadState = BanDatabaseLoadState::Uninitialized;
        mNextRetryTick = 0;
    }
};

BanDatabase &GetBanDatabase() {
    static BanDatabase database;
    return database;
}

} // namespace

int BanDatabasePriority(bool targetsZombies, SeedType seed, std::uint32_t tick) {
    return GetBanDatabase().Priority(targetsZombies, seed, tick);
}

BanDatabaseLoadState GetBanDatabaseLoadState() {
    return GetBanDatabase().LoadState();
}

void ResetBanDatabase() {
    GetBanDatabase().Reset();
}

bool IsPlantTempoMushroom(SeedType seed) {
    return seed == SeedType::SEED_PUFFSHROOM;
}

bool IsPlantCarrySeed(SeedType seed) {
    if (IsPlantTempoMushroom(seed)) {
        return false;
    }
    switch (seed) {
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_CACTUS:
        case SeedType::SEED_CABBAGEPULT:
        case SeedType::SEED_KERNELPULT:
        case SeedType::SEED_MELONPULT:
        case SeedType::SEED_BLOOMERANG:
        case SeedType::SEED_STARFRUIT:
        case SeedType::SEED_SCAREDYSHROOM:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_SPORESHROOM:
            return true;
        default:
            return false;
    }
}

bool IsPeaMainDamageSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_PEASHOOTER:
        case SeedType::SEED_REPEATER:
        case SeedType::SEED_THREEPEATER:
        case SeedType::SEED_SPLITPEA:
        case SeedType::SEED_GATLINGPEA:
            return true;
        default:
            return false;
    }
}

bool IsCoffeeDependentPlant(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_PUFFSHROOM:
        case SeedType::SEED_SCAREDYSHROOM:
        case SeedType::SEED_FUMESHROOM:
        case SeedType::SEED_GLOOMSHROOM:
        case SeedType::SEED_SPORESHROOM:
        case SeedType::SEED_HYPNOSHROOM:
        case SeedType::SEED_ICESHROOM:
        case SeedType::SEED_DOOMSHROOM:
        case SeedType::SEED_MAGNETSHROOM:
            return true;
        default:
            return false;
    }
}

bool IsMagnetTargetZombieSeed(SeedType seed) {
    switch (seed) {
        case SeedType::SEED_ZOMBIE_PAIL:
        case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
        case SeedType::SEED_ZOMBIE_FOOTBALL:
        case SeedType::SEED_ZOMBIE_JACK_IN_THE_BOX:
        case SeedType::SEED_ZOMBIE_DIGGER:
        case SeedType::SEED_ZOMBIE_POGO:
        case SeedType::SEED_ZOMBIE_LADDER:
        case SeedType::SEED_ZOMBIE_TRASHCAN:
            return true;
        default:
            return false;
    }
}

} // namespace vsai::draft
