#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

namespace playback
{

    // Structure to store segment information
    class SegmentInfo
    {
    public:
        SegmentInfo();
        ~SegmentInfo();
        // Function to process AVFrame and calculate statistics
        void processFrame(AVFrame *frame, AVRational time_base);
        void print() const;

    public:
        std::string filename;
        long created_timestamp;
        // Used to check if the segment has been retreived by getLatestSegments function of LogParser
        bool printed = false;

        double pts_average_diff = 0;
        double average_fps = 0;
        // total segment playback duration
        long   decode_time = 0;
        int    num_frames = 0;
        std::vector<long> pts_list;

    private:
        void calculateStatistics();
    };

    class LogParser
    {
    public:
        static LogParser *getInstance();
        std::vector<const SegmentInfo*>  getLatestSegments();
        SegmentInfo* getcurrentSegment();

    private:
        LogParser();
        ~LogParser();

    private:
        std::string log_filename_;
        std::ofstream log_file_;
        std::vector<SegmentInfo *> segments_;
        bool debug_mode = false;

        // Custom log callback to capture segment details
        static void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list args);
        void parse_log(const std::string &log_line);
    };

    static LogParser *log_parser_instance = nullptr;

} // namespace playback

#endif // LOG_PARSER_H
