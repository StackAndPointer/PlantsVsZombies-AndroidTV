#ifndef PVZ_REPLAY_SYSTEM_H
#define PVZ_REPLAY_SYSTEM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class ReplayPacketDir : std::uint8_t {
    Outbound = 0,
    InboundClient = 1,
    InboundServer = 2,
};

struct ReplayMetaInfo {
    std::string filePath;
    std::string fileName;
    std::string hostName;
    std::string guestName;
    std::string winnerName;
    std::string mapName;
    std::string createdAt;
    std::string hostCamp;
    std::string guestCamp;
    std::string plantDeck;
    std::string zombieDeck;
    int netplayVersion = 0;
    int vsBackground = 0;
    int durationTicks = 0;
    std::size_t packetCount = 0;
};

namespace replay {
void ResetRecorder();
void RecordPacket(ReplayPacketDir dir, const std::byte *data, std::size_t len, std::uint32_t tick);
bool SaveCurrentMatchReplay(const ReplayMetaInfo &meta);
std::vector<ReplayMetaInfo> ListReplayFiles();
bool BeginPlaybackFromFile(const std::string &path);
void AdvancePlaybackTick();
int EstimateRecordedDurationTicks();
void StopPlayback();
bool IsPlaybackActive();
void TickPlayback();
} // namespace replay

#endif // PVZ_REPLAY_SYSTEM_H
