#include "hls_parser.hpp"
#include "constants.hpp"
#include "logger.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <cctype>

#include <curl/curl.h>

using namespace playback;

constexpr const char *MP_TAG = "HLSManifestParser";

const std::string EXT_X_TARGETDURATION = "#EXT-X-TARGETDURATION:";
const std::string EXT_X_MEDIA_SEQUENCE = "#EXT-X-MEDIA-SEQUENCE:";
const std::string EXTINF = "#EXTINF:";
const std::string EXT_X_DISCONTINUITY = "#EXT-X-DISCONTINUITY:";
const std::string EXT_X_STREAM_INF = "#EXT-X-STREAM-INF:";
const std::string EXTM3U = "#EXTM3U";


HLSManifestParser::HLSManifestParser(const std::string uri, int refresh_interval) : uri(uri), refresh_interval(refresh_interval)
{
    curl_global_init(CURL_GLOBAL_DEFAULT); // Initialize global state for libcurl
}

HLSManifestParser::~HLSManifestParser()
{
    if (parsingThread.joinable())
    {
        parsingThread.join();
    }
}

// Start parsing in a separate thread
void HLSManifestParser::startParsing()
{
    parsingThread = std::thread(&HLSManifestParser::parseFromURI, this, uri);
}

// Wait for the parsing thread to complete
void HLSManifestParser::waitForCompletion()
{
    std::unique_lock<std::mutex> lock(dataMutex);
    parsingComplete.wait(lock, [this]
                         { return isParsingDone; });
    if (parsingThread.joinable())
    {
        parsingThread.join();
    }
}

// Callback function for libcurl to write data into a string
static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

void HLSManifestParser::parseFromURI(const std::string &uri)
{
    int loops = 0;
    while (!isParsingDone)
    {
        try
        {
            Logger::getInstance().log("Fetching main manifest: " + uri + ", loop: " + std::to_string(loops), Logger::Severity::DEBUG, MP_TAG);
            std::string manifest = fetchContentFromURI(uri);
            if (manifest.length() > 10)
            {
                if (started_timestamp == -1) {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    started_timestamp = get_utc();
                }
                parse(manifest);
            }
            std::this_thread::sleep_for(std::chrono::seconds(refresh_interval));
            loops++;
        }
        catch (const std::exception &ex)
        {
            Logger::getInstance().log("Error: " + std::string(ex.what()), Logger::Severity::ERROR, MP_TAG);
            std::this_thread::sleep_for(std::chrono::seconds(refresh_interval));
        }
    }
    // Notify that parsing is done
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        isParsingDone = true;
    }
    parsingComplete.notify_all();
}

std::string HLSManifestParser::fetchContentFromURI(const std::string &uri)
{
    std::string response;
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize CURL");
    }
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_NONE); // Ensure no auth is used
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK)
    {
        Logger::getInstance().log("We got error: " + std::to_string(res) + ", fetching: " + uri, Logger::Severity::ERROR, MP_TAG);
        return "";
    }
    // Save the base URI
    auto lastSlash = uri.find_last_of('/');
    if (lastSlash != std::string::npos)
    {
        baseUri = uri.substr(0, lastSlash + 1);
    }
    else
    {
        baseUri = uri; // Handle edge case where URI has no '/'
    }
    return response;
}

// Parse the HLS manifest string
void HLSManifestParser::parse(const std::string &manifest)
{
    Logger::getInstance().log("Parsing manifest ...", Logger::Severity::DEBUG, MP_TAG);
    std::istringstream stream(manifest);
    std::string line;
    std::shared_ptr<HLSSegment> currentSegment = std::make_shared<HLSSegment>();
    while (std::getline(stream, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#')
        {
            // Process HLS tags
            if (line.rfind(EXTM3U, 0) == 0)
            {
                continue; // Playlist header
            }
            else if (line.rfind(EXT_X_STREAM_INF, 0) == 0)
            {
                masterPlaylist = true;
                HLSVariantStream variantStream;
                std::istringstream tagStream(line.substr(18)); // Skip "#EXT-X-STREAM-INF:"
                std::string token;
                while (std::getline(tagStream, token, ','))
                {
                    auto pos = token.find('=');
                    if (pos != std::string::npos)
                    {
                        std::string key = trim(token.substr(0, pos));
                        std::string value = trim(token.substr(pos + 1));

                        if (key == "BANDWIDTH")
                        {
                            variantStream.bandwidth = std::stoi(value);
                        }
                        else if (key == "RESOLUTION")
                        {
                            // TODO: Parse resolution
                            // variantStream.resolution = value;
                        }
                    }
                }
                variantStream.uri = resolveUri(stream);
                std::lock_guard<std::mutex> lock(dataMutex);
                variantStreams.push_back(variantStream);
            }
            else if (line.rfind(EXT_X_TARGETDURATION, 0) == 0)
            {
                Logger::getInstance().log("Extractng duration from " + line, Logger::Severity::DEBUG, MP_TAG);
                std::lock_guard<std::mutex> lock(dataMutex);
                target_duration = std::stol(line.substr(EXT_X_TARGETDURATION.length())); // Skip "#EXT-X-TARGETDURATION:"
                refresh_interval = target_duration / 2;
            }
            else if (line.rfind(EXT_X_MEDIA_SEQUENCE, 0) == 0)
            {
                Logger::getInstance().log("Extractng media sequence from " + line, Logger::Severity::DEBUG, MP_TAG);
                std::lock_guard<std::mutex> lock(dataMutex);
                media_sequence = std::stoi(line.substr(EXT_X_MEDIA_SEQUENCE.length())); // Skip "#EXT-X-MEDIA-SEQUENCE:"
            }
            else if (line.rfind(EXT_X_DISCONTINUITY, 0) == 0)
            {
                discontinuetym = true;
                masterPlaylist = true;
            }
            else if (line.rfind(EXTINF, 0) == 0)
            {
                Logger::getInstance().log("Extractng #EXTINF from " + line, Logger::Severity::DEBUG, MP_TAG);
                currentSegment->setDeclaredDuration(std::stod(line.substr(EXTINF.length())));
                currentSegment->setUri(resolveUri(stream));
                if (currentSegment->getUri() == baseUri)
                {
                    continue;
                }
                else
                {
                    int sequence_number = extract_sequence_number(currentSegment->getUri());
                    std::lock_guard<std::mutex> lock(dataMutex);
                    Logger::getInstance().log("Looking for latest doqnloaded sequence ...", Logger::Severity::DEBUG, MP_TAG);
                    int last_sequence_number = -1;
                    if (segments.size() > 0) {
                        last_sequence_number = segments.back()->getSequenceNumber();
                    }
                    Logger::getInstance().log("Checking already downloaded segments for sequence num: " + std::to_string(sequence_number) 
                            + ", latest: " + std::to_string(last_sequence_number), Logger::Severity::DEBUG, MP_TAG);
                    if (last_sequence_number < sequence_number)
                    {
                        Logger::getInstance().log("Adding segment to segments ...", Logger::Severity::DEBUG, MP_TAG);
                        segments.push_back(currentSegment);
                        Logger::getInstance().log("Creating decoder ...", Logger::Severity::DEBUG, MP_TAG);
                        std::unique_ptr<Decoder> dec = std::make_unique<Decoder>(currentSegment);
                        Logger::getInstance().log("Pushing decoder to segments_decoders ...", Logger::Severity::DEBUG, MP_TAG);
                        segments_decoders.push_back(std::move(dec));
                        Logger::getInstance().log("Making new current segment shared pointer ...", Logger::Severity::DEBUG, MP_TAG);
                        currentSegment = std::make_shared<HLSSegment>(); // Reset for the next segment
                    }
                    else
                    {
                        Logger::getInstance().log("Skipping already parsed segment: " + currentSegment->getUri() + ", with sequence number: " + std::to_string(sequence_number) + ", lates seq num: " + std::to_string(segments.back()->getSequenceNumber()),
                                                  Logger::Severity::DEBUG, MP_TAG);
                        continue;
                    }
                }
            }
        }
        // else
        // {
        //     // Process segment URI
        // }
    }
}

// Resolve relative URI to absolute
std::string HLSManifestParser::resolveUri(std::istringstream &stream)
{
    std::string line;
    // Next line contains the URI for the variant stream
    if (std::getline(stream, line))
    {
        Logger::getInstance().log("Extractng url from: " + line, Logger::Severity::DEBUG, MP_TAG);
        std::string relative = trim(line);

        // Check if the URI is already absolute
        if (std::regex_match(relative, std::regex(R"(https?://.*)")))
        {
            Logger::getInstance().log("uri: " + relative, Logger::Severity::DEBUG, MP_TAG);
            return relative;
        }
        // Otherwise, combine the base and relative URI
        if (relative.empty())
        {
            Logger::getInstance().log("uri: " + baseUri, Logger::Severity::DEBUG, MP_TAG);
            return baseUri; // Handle edge case
        }
        if (baseUri.back() == '/' || relative.front() == '/')
        {
            Logger::getInstance().log("uri: " + baseUri + relative, Logger::Severity::DEBUG, MP_TAG);
            return baseUri + relative;
        }
        Logger::getInstance().log("uri: " + baseUri + "/" + relative, Logger::Severity::DEBUG, MP_TAG);
        return baseUri + "/" + relative;
    }
    else
    {
        Logger::getInstance().log("uri: nullptr", Logger::Severity::ERROR, MP_TAG);
        return nullptr;
    }
}

// Helper function to trim whitespace
std::string HLSManifestParser::trim(const std::string &str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    return (start == std::string::npos || end == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Get the list of media segments
std::vector<std::shared_ptr<HLSSegment>> HLSManifestParser::getSegments()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return segments;
}

// Get the list of variant streams
std::vector<HLSVariantStream> HLSManifestParser::getVariantStreams()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return variantStreams;
}

// Check if the manifest is a master playlist
bool HLSManifestParser::isMasterPlaylist()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return masterPlaylist;
}

long HLSManifestParser::getTotalRunningTime() {
    std::lock_guard<std::mutex> lock(dataMutex);
    long total_runtime = 0;
    for (auto segment : segments) {
        if (segment->getStatus() == SegmentStatus::DOWNLOADED) {
            // Logger::getInstance().log("Segment used to calculate runtime, seq name: " + std::to_string(segment->getSequenceNumber()), Logger::Severity::DEBUG, HLS_TAG);
            total_runtime = get_utc() - segment->getStartedTimstamp();
            break;
        }
    }
    return total_runtime;
}

long HLSManifestParser::getTotalDecodeTime() {
    std::lock_guard<std::mutex> lock(dataMutex);
    long total_decode_time = 0;
    for (auto segment : segments) {
        total_decode_time += segment->getDecodeDuration();
    }
    return total_decode_time;
}

long HLSManifestParser::getTotalDeclaredTime() {
    std::lock_guard<std::mutex> lock(dataMutex);
    long total_declared_time = 0;
    for (auto segment : segments) {
        total_declared_time += static_cast<long>(segment->getDeclaredTime() * 1000);
    }
    return total_declared_time;
}

long HLSManifestParser::getTargetDuration() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return target_duration;
}