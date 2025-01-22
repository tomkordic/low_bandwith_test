extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
#include <libavutil/log.h>
}

#include <iostream>
#include <sstream> 
#include <vector>
#include <thread>

#include "constants.hpp"
#include "hls_parser.hpp"
#include "logger.hpp"

using namespace playback;

constexpr const char* MAIN_TAG = "Main Thread";

void check_non_increasing_pts(const std::vector<long> &pts_list)
{
  if (!std::is_sorted(pts_list.begin(), pts_list.end()))
  {

    std::ostringstream msg;
    msg << "ERROR: non-increasing pts: ";
    for (auto pts : pts_list)
    {
      msg << pts << ", ";
    }
    Logger::getInstance().log(msg, Logger::Severity::ERROR, MAIN_TAG);
  }
}

void check_pts_gaps(const std::vector<long> &pts_list, int max_allowed)
{
  if (pts_list.size() < 2)
  {
    return;
  }
  long prev_pts = pts_list[0];
  std::ostringstream msg;
  for (size_t i = 1; i < pts_list.size(); i++)
  {
    if (pts_list[i] - prev_pts > max_allowed)
    {
      msg << "pts gap: " << pts_list[i] - prev_pts << " ms, is larger than: " << max_allowed << "this will cause a playback freeze";
      Logger::getInstance().log(msg, Logger::Severity::ERROR, MAIN_TAG);
    }
    prev_pts = pts_list[i];
  }
}

int main(int argc, char *argv[])
{
  Logger::getInstance().log("\n\n====== PLAYBACK PARSER ======\n\n", Logger::Severity::INFO, MAIN_TAG);
  if (argc < 2)
  {
    std::ostringstream msg;
    msg << "Usage: " << argv[0] << " <video_file/uri>";
    Logger::getInstance().log(msg, Logger::Severity::INFO, MAIN_TAG);
    return -1;
  }
  av_log_set_level(AV_LOG_QUIET);
  Logger::getInstance().setLogFile("playback.log");
  // Logger::getInstance().setLogLevel(Logger::Severity::DEBUG);
  const char *uri = argv[1];

  HLSManifestParser parser(uri);

  // Decode frames
  Logger::getInstance().log("Decoding stream.", Logger::Severity::INFO, HLS_TAG);
  parser.startParsing();
  try
  {
    while (true)
    {
      std::this_thread::sleep_for(std::chrono::seconds(3));
      Logger::getInstance().log("Received segments:", Logger::Severity::INFO, HLS_TAG);
      for(std::shared_ptr<HLSSegment> segment : parser.getSegments())
      {
        if (segment->getStatus() == SegmentStatus::DOWNLOADED && !segment->isPrinted()) {
          segment->print("  ");
          check_non_increasing_pts(segment->getPtsList());
          check_pts_gaps(segment->getPtsList(), segment->getAveragePtsDiff() * 3);
        }
      }
      long runtime = parser.getTotalRunningTime();
      long decode_time = parser.getTotalDecodeTime();
      long declared_time = parser.getTotalDeclaredTime();
      std::ostringstream msg;
      if (runtime == 0) {
        msg << "Latency check:\n"
            << " Waiting for segments ...\n"; 
        Logger::getInstance().log(msg, Logger::Severity::INFO, HLS_TAG);
        continue;
      } 
      msg << "Latency check:\n"
          << " Runtime: " << runtime << "ms\n" 
          << " total buffered(decoded time): " << decode_time << "ms\n"
          << " total declared time(from manifest): " << declared_time << "ms\n"
          << " real to dec time diff: " << (decode_time - runtime);
      Logger::getInstance().log(msg, Logger::Severity::INFO, HLS_TAG);
      if (runtime > decode_time) {
        Logger::getInstance().log("Missing playback time: " + std::to_string(decode_time - runtime), Logger::Severity::ERROR, HLS_TAG);
      }
      if (abs(decode_time - declared_time) > ((parser.getTargetDuration() + 1) * 1000)) {
        std::ostringstream msg;
        msg << "  Declared time do not match decoded time: \n" 
            << "  diff: " << (decode_time - declared_time) << "\n"
            << "  target duration: " << parser.getTargetDuration() << "\n";
        Logger::getInstance().log(msg, Logger::Severity::WARNING, HLS_TAG);
      }
      Logger::getInstance().log("\n\n    =========================== \n\n", Logger::Severity::INFO, HLS_TAG);
    }
  }
  catch (std::runtime_error &e)
  {
    std::ostringstream msg;
    msg << "ERROR: " << e.what();
    Logger::getInstance().log(msg, Logger::Severity::ERROR, MAIN_TAG);
  }

  Logger::getInstance().log("Finished processing stream.", Logger::Severity::INFO, MAIN_TAG);
  return 0;
}