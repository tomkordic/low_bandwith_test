#ifndef HLS_MANIFEST_PARSER_HPP
#define HLS_MANIFEST_PARSER_HPP

extern "C"
{
#include <libavformat/avformat.h>
}

#include "hls_segment.hpp"
#include "decoder.hpp"

#include <string>
#include <vector>
#include <optional>
#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>

#include <curl/curl.h>

namespace playback
{
    // Represents a variant stream in the HLS manifest
    struct HLSVariantStream
    {
        std::string uri;
        int bandwidth = 0;
        int res_width = -1;
        int res_height = -1;
    };

    // Main HLS manifest parser class
    class HLSManifestParser
    {
    public:
        // Constructor and Destructor
        HLSManifestParser(const std::string uri, int refresh_interval = 3);
        ~HLSManifestParser();

        // Start parsing in a separate thread
        void startParsing();

        // Wait for the parsing thread to complete
        void waitForCompletion();

        // Get the list of media segments
        std::vector<std::shared_ptr<HLSSegment>> getSegments();

        // Get the list of variant streams (if any)
        std::vector<HLSVariantStream> getVariantStreams();

        // Check if the manifest is a master playlist
        bool isMasterPlaylist();

        long getTotalRunningTime();

        long getTotalDecodeTime();

        long getTotalDeclaredTime();

        long getTargetDuration();
    private:
        void parseFromURI(const std::string &uri); // Fetch and parse manifest from URI
        std::string fetchContentFromURI(const std::string &uri);
        void parse(const std::string &manifest);

    private:
        std::vector<std::unique_ptr<Decoder>> segments_decoders;
        std::vector<std::shared_ptr<HLSSegment>> segments;
        std::vector<HLSVariantStream> variantStreams;
        const std::string uri;
        std::string baseUri;
        bool masterPlaylist = true;
        bool isLive = false;
        int protocol_version = 3;
        int media_sequence = 0;
        long target_duration = 0;
        bool discontinuetym = false;
        // TS when master playlist was pooled first time
        long started_timestamp = -1;

        std::thread parsingThread;
        std::mutex dataMutex;
        std::condition_variable parsingComplete;
        bool isParsingDone = false;
        int refresh_interval = 0;
        CURL *curl = nullptr;

    private:
        // Helper function to trim whitespace from a string
        std::string resolveUri(std::istringstream &stream);
        std::string trim(const std::string &str);
    };
} // namespace playback

#endif // HLS_MANIFEST_PARSER_HPP
