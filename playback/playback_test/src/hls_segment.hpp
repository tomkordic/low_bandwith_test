#ifndef HLS_SEGMENT_HPP
#define HLS_SEGMENT_HPP

extern "C"
{
#include <libavformat/avformat.h>
}

#include "constants.hpp"
#include "logger.hpp"

#include <vector>
#include <numeric>
#include <algorithm>
#include <thread>
#include <mutex>

namespace playback
{
    constexpr const char *HLS_TAG = "HLSSegment";

    enum SegmentStatus
    {
        IN_PROGRESS,
        DOWNLOADED,
        DOWNLOAD_FAILED
    };

    // Represents a media segment in the HLS manifest
    class HLSSegment
    {
    private:
        std::mutex dataMutex;
        std::string uri;
        long started_timestamp;
        // Used to check if the segment has been retreived by getLatestSegments function of LogParser
        bool printed = false;
        long sequence_number = -1;

        double declared_duration = 0.0;
        double pts_average_diff = 0;
        double average_fps = 0;
        // total segment playback duration
        long decode_duration = 0;
        int num_frames = 0;
        std::vector<long> pts_list;
        SegmentStatus status = SegmentStatus::IN_PROGRESS;

    public:
        HLSSegment()
        {
            started_timestamp = get_utc();
        }
        inline void calculateStatistics(AVFrame *frame, AVRational time_base)
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            if (frame->pts != AV_NOPTS_VALUE)
            {
                pts_list.push_back(pts_to_ms(frame, time_base));
            }
            else
            {
                Logger::getInstance().log("Failed to obtain pts for frame", Logger::Severity::ERROR, HLS_TAG);
            }
            decode_duration = pts_list.back() - pts_list.front();
            num_frames++;
            if (pts_list.size() > 1)
            {
                std::vector<int64_t> diffs(pts_list.size() - 1);
                for (size_t i = 1; i < pts_list.size(); ++i)
                {
                    diffs[i - 1] = pts_list[i] - pts_list[i - 1];
                }
                pts_average_diff = std::accumulate(diffs.begin(), diffs.end(), 0.0) / diffs.size();
            }

            average_fps = (double)num_frames / (double)decode_duration;
        }
        inline void download_complete()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            status = SegmentStatus::DOWNLOADED;
        }
        inline void download_failed()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            status = SegmentStatus::DOWNLOAD_FAILED;
        }
        inline void print(std::string prefix = "")
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            Logger::getInstance().log(prefix + "Segment: " + uri, Logger::Severity::INFO, HLS_TAG);
            Logger::getInstance().log(prefix + "  Status: " + segmentStatusToString(status), Logger::Severity::INFO, HLS_TAG);
            Logger::getInstance().log(prefix + "  Created at: " + std::to_string(started_timestamp), Logger::Severity::INFO, HLS_TAG);
            Logger::getInstance().log(prefix + "  Number of frames: " + std::to_string(num_frames), Logger::Severity::INFO, HLS_TAG);
            Logger::getInstance().log(prefix + "  Average FPS: " + std::to_string(average_fps), Logger::Severity::INFO, HLS_TAG);
            Logger::getInstance().log(prefix + "  PTS average diff: " + std::to_string(pts_average_diff) + " ms", Logger::Severity::INFO, HLS_TAG);
            Logger::getInstance().log(prefix + "  Decode time: " + std::to_string(decode_duration) + " ms", Logger::Severity::INFO, HLS_TAG);
            Logger::getInstance().log(prefix + "  Declared time: " + std::to_string(static_cast<long>(declared_duration * 1000)) + " ms", Logger::Severity::INFO, HLS_TAG);
            printed = true;
        }
        inline const char *segmentStatusToString(SegmentStatus status)
        {
            switch (status)
            {
            case IN_PROGRESS:
                return "IN_PROGRESS";
            case DOWNLOADED:
                return "DOWNLOADED";
            case DOWNLOAD_FAILED:
                return "DOWNLOAD_FAILED";
            default:
                return "UNKNOWN";
            }
        }
        inline SegmentStatus getStatus()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return status;
        }
        inline bool isPrinted()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return printed;
        }
        inline std::vector<long> getPtsList()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return pts_list;
        }
        inline double getAveragePtsDiff()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return pts_average_diff;
        }
        inline std::string getUri()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return uri;
        }
        inline void setUri(std::string val)
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            sequence_number = extract_sequence_number(val);
            uri = val;
        }
        inline double getDeclaredDuration()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return declared_duration;
        }
        inline void setDeclaredDuration(double val)
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            declared_duration = val;
        }
        inline int getSequenceNumber()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return sequence_number;
        }
        inline void setSequenceNumber(int val)
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            sequence_number = val;
        }
        inline long getDecodeDuration()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return decode_duration;
        }
        inline double getDeclaredTime()
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            return declared_duration;
        }
        inline long getStartedTimstamp() {
            std::lock_guard<std::mutex> lock(dataMutex);
            return started_timestamp;
        }
        inline int getNumFrames() {
            std::lock_guard<std::mutex> lock(dataMutex);
            return num_frames;
        }
        inline void updateStartedTimestamp() {
            std::lock_guard<std::mutex> lock(dataMutex);
            started_timestamp = get_utc();
        }
    };
} // namespace playback

#endif // HLS_SEGMENT_HPP