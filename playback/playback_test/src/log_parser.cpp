extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/time.h>
}

#include "log_parser.hpp"
#include <iostream>
#include <numeric>
#include <cstring>
#include <regex>

#include "constants.hpp"

using namespace playback;

SegmentInfo::SegmentInfo()
{
    created_timestamp = get_utc();
}

SegmentInfo::~SegmentInfo()
{
}

void SegmentInfo::processFrame(AVFrame *frame, AVRational time_base)
{
    if (frame->pts != AV_NOPTS_VALUE)
    {
        pts_list.push_back(pts_to_ms(frame, time_base));
    } else {
        std::cout << RED << "Failed to obtain pts for frame" << RESET << std::endl;
    }
    decode_time = pts_list.back() - pts_list.front();
    num_frames++;
    // Update statistics after each frame
    calculateStatistics();
}

void SegmentInfo::calculateStatistics()
{
    if (pts_list.size() > 1)
    {
        std::vector<int64_t> diffs(pts_list.size() - 1);
        for (size_t i = 1; i < pts_list.size(); ++i)
        {
            diffs[i - 1] = pts_list[i] - pts_list[i - 1];
        }
        pts_average_diff = std::accumulate(diffs.begin(), diffs.end(), 0.0) / diffs.size();
    }

    average_fps = (double)num_frames / (double)decode_time;
}

void SegmentInfo::print() const
{
    std::cout << "Segment: " << filename << std::endl;
    std::cout << "  Created at: " << created_timestamp << std::endl;
    std::cout << "  Number of frames: " << num_frames << std::endl;
    std::cout << "  Average FPS: " << average_fps << std::endl;
    std::cout << "  Decode time: " << decode_time << " ms" << std::endl;
    std::cout << "  PTS average diff: " << pts_average_diff << " ms" << std::endl;
}

LogParser::LogParser()
{
    std::cout << "LogParser init" << std::endl;
    // Open the log file in append mode
    log_filename_ = "./ffmpeg.log";
    if (debug_mode)
    {
        log_file_.open(log_filename_, std::ios::app);
        if (!log_file_.is_open())
        {
            std::cerr << "Failed to open log file" << std::endl;
            return;
        }
    }
    // Set FFmpeg log callback
    av_log_set_callback(ffmpeg_log_callback);
    av_log_set_level(AV_LOG_VERBOSE);
}

LogParser::~LogParser()
{
    // Close the log file if it is open
    if (log_file_.is_open())
    {
        log_file_.close();
    }
}

LogParser *LogParser::getInstance()
{
    if (log_parser_instance == nullptr)
    {
        log_parser_instance = new LogParser();
    }
    return log_parser_instance;
}

void LogParser::ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list args)
{
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    std::string log_line(buffer);

    LogParser *parser = LogParser::getInstance();
    // Parse and extract relevant information from the log line
    parser->parse_log(log_line);
    if (parser->log_file_.is_open() && parser->debug_mode)
    {
        parser->log_file_ << log_line;
    }
}

void LogParser::parse_log(const std::string &log_line)
{

    std::smatch match;

    // Extract segment filename
    // if (strstr(log_line.c_str(), "Opening") && (strstr(log_line.c_str(), ".ts") || strstr(log_line.c_str(), ".m3u8")))
    if (strstr(log_line.c_str(), "Opening") && (strstr(log_line.c_str(), ".ts")))
    {
        if (std::regex_search(log_line, match, url_regex))
        {
            SegmentInfo *segment = new SegmentInfo();
            segment->filename = match[0];
            segments_.push_back(segment);
        }
        else
        {
            std::cout << RED << "No URL found in the log line: " << log_line << RESET << std::endl;
        }
    }

    // // Extract segment start time
    // if (strstr(log_line.c_str(), "format: start_time:")) {
    //     if (std::regex_search(log_line, match, decimal_regex)) {
    //         if (!segments_.empty())
    //         {
    //             segments_.back()->start_time = std::stod(match[0]);
    //         }
    //     } else {
    //         std::cout << "No decimal number found in the log line." << std::endl;
    //     }
    // }
}

SegmentInfo* LogParser::getcurrentSegment(){
    if (segments_.empty())
    {
        return nullptr;
    }
    return segments_.back();
}

std::vector<const SegmentInfo*> LogParser::getLatestSegments()
{
    std::vector<const SegmentInfo*> result;
    for (auto segment : segments_)
    {
        if (segment->printed)
        {
            continue;
        }
        result.push_back(segment);
        segment->printed = true;
    }
}

// void LogParser::print_segments() const
// {
//     std::cout << "Downloaded HLS segments:" << std::endl;
//     for (auto segment : segments_)
//     {
//         if (segment->printed)
//         {
//             continue;
//         }
//         std::cout << "  Filename: " << segment->filename
//                   << ", Start Time: " << segment->start_time
//                   << ", Duration: " << segment->duration << std::endl;
//         segment->printed = true;
//     }
// }
