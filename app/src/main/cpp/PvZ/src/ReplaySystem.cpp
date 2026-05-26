#include "PvZ/ReplaySystem.h"

#include "Homura/Logger.h"
#include "PvZ/GlobalVariable.h"
#include "PvZ/Lawn/LawnApp.h"
#include "PvZ/NetPlay.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {
constexpr std::array<char, 6> kReplayMagic = {'P', 'V', 'R', 'P', '1', '\0'};
constexpr const char *kReplayDir = "replays";

struct ReplayPacketRecord {
    ReplayPacketDir dir;
    std::uint32_t tick;
    std::vector<std::byte> data;
};

struct PlaybackState {
    bool active = false;
    std::uint32_t startTick = 0;
    std::size_t nextIndex = 0;
    std::vector<ReplayPacketRecord> packets;
};

std::vector<ReplayPacketRecord> gRecordedPackets;
PlaybackState gPlaybackState;
std::uint32_t gRecordStartTick = 0;
bool gRecordStartTickReady = false;

std::string BuildMetaText(const ReplayMetaInfo &meta) {
    return "host=" + meta.hostName + "\n" + "guest=" + meta.guestName + "\n" + "winner=" + meta.winnerName + "\n" + "map=" + meta.mapName + "\n" + "vs_background=" + std::to_string(meta.vsBackground)
        + "\n" + "created_at=" + meta.createdAt + "\n" + "host_camp=" + meta.hostCamp + "\n" + "guest_camp=" + meta.guestCamp + "\n" + "plant_deck=" + meta.plantDeck + "\n"
        + "zombie_deck=" + meta.zombieDeck + "\n" + "duration_ticks=" + std::to_string(meta.durationTicks) + "\n" + "netplay_version=" + std::to_string(NETPLAY_VERSION) + "\n";
}

std::string ReadMetaValue(const std::string &meta, const char *key) {
    const std::string needle = std::string(key) + "=";
    const std::size_t pos = meta.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    const std::size_t lineEnd = meta.find('\n', pos);
    const std::size_t valueStart = pos + needle.size();
    std::string value = meta.substr(valueStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - valueStart);
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == '\t' || value.back() == ' ')) {
        value.pop_back();
    }
    return value;
}

void CopyNameToGlobal(char *dst, std::size_t dstLen, const std::string &src) {
    if (dst == nullptr || dstLen == 0) {
        return;
    }
    std::strncpy(dst, src.c_str(), dstLen - 1);
    dst[dstLen - 1] = '\0';
}

std::string EscapePrintable(const char *src) {
    if (src == nullptr) {
        return {};
    }
    std::string s(src);
    for (char &c : s) {
        if (c == '\r' || c == '\n' || c == '\t') {
            c = ' ';
        }
    }
    return s;
}

} // namespace

void replay::ResetRecorder() {
    gRecordedPackets.clear();
    gRecordStartTick = 0;
    gRecordStartTickReady = false;
}

void replay::RecordPacket(ReplayPacketDir dir, const std::byte *data, std::size_t len, std::uint32_t tick) {
    if (gPlaybackState.active) {
        return;
    }
    if (data == nullptr || len == 0) {
        return;
    }
    ReplayPacketRecord record;
    record.dir = dir;
    if (!gRecordStartTickReady) {
        gRecordStartTick = tick;
        gRecordStartTickReady = true;
    }
    record.tick = tick - gRecordStartTick;
    record.data.assign(data, data + len);
    gRecordedPackets.push_back(std::move(record));
}

bool replay::SaveCurrentMatchReplay(const ReplayMetaInfo &meta) {
    namespace fs = std::filesystem;
    if (gRecordedPackets.empty()) {
        LOG_WARN("[REPLAY] skip save: no packet recorded");
        return false;
    }
    const std::vector<ReplayPacketRecord> &packetsToSave = gRecordedPackets;
    std::error_code ec;
    fs::create_directories(kReplayDir, ec);
    if (ec) {
        LOG_ERROR("[REPLAY] create dir failed: {}", ec.message());
        return false;
    }

    const std::string outPath = std::string(kReplayDir) + "/" + meta.fileName;
    std::ofstream out(outPath, std::ios::binary);
    if (!out.is_open()) {
        LOG_ERROR("[REPLAY] open failed path={}", outPath);
        return false;
    }

    const std::string metaText = BuildMetaText(meta);
    const std::uint32_t metaLen = static_cast<std::uint32_t>(metaText.size());
    const std::uint32_t packetCount = static_cast<std::uint32_t>(packetsToSave.size());
    out.write(kReplayMagic.data(), kReplayMagic.size());
    out.write(reinterpret_cast<const char *>(&metaLen), sizeof(metaLen));
    out.write(metaText.data(), static_cast<std::streamsize>(metaText.size()));
    out.write(reinterpret_cast<const char *>(&packetCount), sizeof(packetCount));
    for (const auto &p : packetsToSave) {
        const std::uint8_t dir = static_cast<std::uint8_t>(p.dir);
        const std::uint32_t len = static_cast<std::uint32_t>(p.data.size());
        out.write(reinterpret_cast<const char *>(&dir), sizeof(dir));
        out.write(reinterpret_cast<const char *>(&p.tick), sizeof(p.tick));
        out.write(reinterpret_cast<const char *>(&len), sizeof(len));
        out.write(reinterpret_cast<const char *>(p.data.data()), static_cast<std::streamsize>(p.data.size()));
    }
    out.close();
    LOG_INFO("[REPLAY] saved path={} packets={}", outPath, packetsToSave.size());
    return true;
}

std::vector<ReplayMetaInfo> replay::ListReplayFiles() {
    namespace fs = std::filesystem;
    std::vector<ReplayMetaInfo> infos;
    std::error_code ec;
    if (!fs::exists(kReplayDir, ec)) {
        return infos;
    }
    for (const auto &entry : fs::directory_iterator(kReplayDir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".rpl") {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        if (!in.is_open()) {
            continue;
        }
        std::array<char, 6> magic{};
        in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        if (magic != kReplayMagic) {
            continue;
        }
        std::uint32_t metaLen = 0;
        in.read(reinterpret_cast<char *>(&metaLen), sizeof(metaLen));
        if (metaLen == 0 || metaLen > 64 * 1024) {
            continue;
        }
        std::string meta(metaLen, '\0');
        in.read(meta.data(), static_cast<std::streamsize>(metaLen));
        std::uint32_t packetCount = 0;
        in.read(reinterpret_cast<char *>(&packetCount), sizeof(packetCount));

        ReplayMetaInfo info;
        info.filePath = entry.path().string();
        info.fileName = entry.path().filename().string();
        info.hostName = ReadMetaValue(meta, "host");
        info.guestName = ReadMetaValue(meta, "guest");
        info.winnerName = ReadMetaValue(meta, "winner");
        info.mapName = ReadMetaValue(meta, "map");
        info.vsBackground = std::atoi(ReadMetaValue(meta, "vs_background").c_str());
        info.createdAt = ReadMetaValue(meta, "created_at");
        info.hostCamp = ReadMetaValue(meta, "host_camp");
        info.guestCamp = ReadMetaValue(meta, "guest_camp");
        info.plantDeck = ReadMetaValue(meta, "plant_deck");
        info.zombieDeck = ReadMetaValue(meta, "zombie_deck");
        info.durationTicks = std::atoi(ReadMetaValue(meta, "duration_ticks").c_str());
        info.netplayVersion = std::atoi(ReadMetaValue(meta, "netplay_version").c_str());
        info.packetCount = packetCount;
        infos.push_back(std::move(info));
    }
    std::sort(infos.begin(), infos.end(), [](const ReplayMetaInfo &a, const ReplayMetaInfo &b) { return a.fileName > b.fileName; });
    return infos;
}

bool replay::BeginPlaybackFromFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        LOG_ERROR("[REPLAY] playback open failed path={}", path);
        return false;
    }
    std::array<char, 6> magic{};
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (magic != kReplayMagic) {
        LOG_ERROR("[REPLAY] invalid magic path={}", path);
        return false;
    }
    std::uint32_t metaLen = 0;
    in.read(reinterpret_cast<char *>(&metaLen), sizeof(metaLen));
    if (metaLen == 0 || metaLen > 64 * 1024) {
        LOG_ERROR("[REPLAY] invalid meta length={} path={}", metaLen, path);
        return false;
    }
    std::string meta(metaLen, '\0');
    in.read(meta.data(), static_cast<std::streamsize>(metaLen));
    const std::string hostName = ReadMetaValue(meta, "host");
    const std::string guestName = ReadMetaValue(meta, "guest");
    std::string hostNameFallback = hostName;
    std::string guestNameFallback = guestName;
    if (hostNameFallback.empty() || guestNameFallback.empty()) {
        const std::string base = std::filesystem::path(path).stem().string(); // replay_xxx_HOST_vs_GUEST
        const std::string marker = "_vs_";
        const std::size_t pos = base.rfind(marker);
        if (pos != std::string::npos) {
            const std::size_t leftUnderscore = base.rfind('_', pos - 1);
            if (leftUnderscore != std::string::npos && leftUnderscore + 1 < pos) {
                if (hostNameFallback.empty()) {
                    hostNameFallback = base.substr(leftUnderscore + 1, pos - (leftUnderscore + 1));
                }
                if (guestNameFallback.empty()) {
                    guestNameFallback = base.substr(pos + marker.size());
                }
            }
        }
    }
    const int replayVersion = std::atoi(ReadMetaValue(meta, "netplay_version").c_str());
    if (replayVersion != 0 && replayVersion != static_cast<int>(NETPLAY_VERSION)) {
        LOG_WARN("[REPLAY] netplay version mismatch replay={} local={}", replayVersion, NETPLAY_VERSION);
    }
    CopyNameToGlobal(gServerHostName, sizeof(gServerHostName), hostNameFallback);
    CopyNameToGlobal(gSecondPlayerName, sizeof(gSecondPlayerName), guestNameFallback);
    CopyNameToGlobal(gReplayHostName, sizeof(gReplayHostName), hostNameFallback);
    CopyNameToGlobal(gReplayGuestName, sizeof(gReplayGuestName), guestNameFallback);
    LOG_INFO("[REPLAY] loaded names global host='{}' guest='{}' replayHost='{}' replayGuest='{}'",
             EscapePrintable(gServerHostName),
             EscapePrintable(gSecondPlayerName),
             EscapePrintable(gReplayHostName),
             EscapePrintable(gReplayGuestName));
    std::uint32_t packetCount = 0;
    in.read(reinterpret_cast<char *>(&packetCount), sizeof(packetCount));

    std::vector<ReplayPacketRecord> packets;
    packets.reserve(packetCount);
    for (std::uint32_t i = 0; i < packetCount; ++i) {
        std::uint8_t dir = 0;
        std::uint32_t tick = 0;
        std::uint32_t len = 0;
        in.read(reinterpret_cast<char *>(&dir), sizeof(dir));
        in.read(reinterpret_cast<char *>(&tick), sizeof(tick));
        in.read(reinterpret_cast<char *>(&len), sizeof(len));
        if (!in.good() || len > 1024 * 1024) {
            LOG_ERROR("[REPLAY] corrupted packet at idx={}", i);
            return false;
        }
        ReplayPacketRecord rec;
        rec.dir = static_cast<ReplayPacketDir>(dir);
        rec.tick = tick;
        rec.data.resize(len);
        in.read(reinterpret_cast<char *>(rec.data.data()), static_cast<std::streamsize>(len));
        if (!in.good()) {
            LOG_ERROR("[REPLAY] read packet payload failed idx={}", i);
            return false;
        }
        packets.push_back(std::move(rec));
    }

    gPlaybackState.active = true;
    gPlaybackState.startTick = gNetPingNowTick;
    gPlaybackState.nextIndex = 0;
    gPlaybackState.packets = std::move(packets);
    LOG_INFO("[REPLAY] playback begin path={} packets={}", path, gPlaybackState.packets.size());
    return true;
}

int replay::EstimateRecordedDurationTicks() {
    if (gRecordedPackets.size() < 2) {
        return 0;
    }
    const std::uint32_t firstTick = gRecordedPackets.front().tick;
    const std::uint32_t lastTick = gRecordedPackets.back().tick;
    return (lastTick >= firstTick) ? int(lastTick - firstTick) : 0;
}

void replay::StopPlayback() {
    if (!gPlaybackState.active) {
        return;
    }
    LOG_INFO("[REPLAY] playback stopped");
    gPlaybackState = {};
    gIsReplayMode = false;
    gReplayPauseByMenu = false;
}

bool replay::IsPlaybackActive() {
    return gPlaybackState.active;
}

void replay::TickPlayback() {
    if (!gPlaybackState.active || gLawnApp == nullptr) {
        return;
    }
    const std::uint32_t elapsedTick = gNetPingNowTick - gPlaybackState.startTick;
    while (gPlaybackState.nextIndex < gPlaybackState.packets.size()) {
        const auto &pkt = gPlaybackState.packets[gPlaybackState.nextIndex];
        if (pkt.tick > elapsedTick) {
            break;
        }
        // Replay is played in spectator-like mode:
        // route all packets through server-path dispatcher so EVENT_CLIENT_*
        // can follow the same spectator branch handling as live spectate.
        if (pkt.dir == ReplayPacketDir::InboundClient || pkt.dir == ReplayPacketDir::InboundServer || pkt.dir == ReplayPacketDir::Outbound) {
            gLawnApp->HandleTcpServerMessage(pkt.data.data(), pkt.data.size());
        }
        ++gPlaybackState.nextIndex;
    }
    if (gPlaybackState.nextIndex >= gPlaybackState.packets.size()) {
        StopPlayback();
    }
}
