#include <chrono>
#include <thread>

#include "decoder.hpp"
#include "constants.hpp"
#include "logger.hpp"

using namespace playback;

constexpr const char* TAG = "Decoder";

Decoder::Decoder(std::shared_ptr<HLSSegment> segment)
    : segment(segment),
      formatContext(nullptr), codecContext(nullptr),
      videoStreamIndex(-1), decoded_frames(0),
      received_packets(0), num_of_failed_frames_in_arrow(0),
      stopDecoding(false), outputQueue(1000), started_at(-1)
{
    // Open input file
    Logger::getInstance().log("Attempting to connect: " + segment->getUri(), Logger::Severity::DEBUG, TAG);
    if (avformat_open_input(&formatContext, segment->getUri().c_str(), nullptr, nullptr) < 0)
    {
        Logger::getInstance().log("Failed to open input file: " + segment->getUri() + ", sleeping 3s", Logger::Severity::ERROR, TAG);
        throw std::runtime_error("Failed to open input file: " + segment->getUri());
    }

    // Retrieve stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0)
    {
        avformat_close_input(&formatContext);
        throw std::runtime_error("Failed to retrieve stream information");
    }

    // Find the first video stream
    for (unsigned int i = 0; i < formatContext->nb_streams; i++)
    {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1)
    {
        avformat_close_input(&formatContext);
        throw std::runtime_error("No video stream found");
    }

    // Get codec parameters and find decoder
    AVCodecParameters *codecParams = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec)
    {
        avformat_close_input(&formatContext);
        throw std::runtime_error("Unsupported codec");
    }

    // Allocate and configure codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext)
    {
        avformat_close_input(&formatContext);
        throw std::runtime_error("Failed to allocate codec context");
    }

    if (avcodec_parameters_to_context(codecContext, codecParams) < 0)
    {
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        throw std::runtime_error("Failed to copy codec parameters to codec context");
    }

    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0)
    {
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        throw std::runtime_error("Failed to open codec");
    }

    // Start the decoding thread
    started_at = get_utc();
    decodingWorker = std::thread(&Decoder::decodingThread, this);
}

Decoder::~Decoder()
{
    // Stop decoding and wait for the thread to finish
    stopDecoding = true;
    if (decodingWorker.joinable())
    {
        decodingWorker.join();
    }

    // Cleanup FFmpeg resources
    if (codecContext)
    {
        avcodec_free_context(&codecContext);
    }
    if (formatContext)
    {
        avformat_close_input(&formatContext);
    }

    // Free remaining packets in the queue
    while (!outputQueue.empty())
    {
        AVFrame *frame = outputQueue.pop();
        av_frame_free(&frame);
    }
}

bool Decoder::isDecoding()
{
    return !stopDecoding;
}

AVFrame *Decoder::getFrame(long timeout_ms)
{
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(timeout_ms))
    {
        if (!outputQueue.empty())
        {
            return outputQueue.pop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Avoid busy-waiting
    }
    return nullptr; // Return nullptr if timeout is reached
}

void Decoder::decodingThread()
{
    while (!stopDecoding)
    {
        AVPacket *packet = av_packet_alloc();
        int ret = av_read_frame(formatContext, packet);
        if (ret >= 0)
        {
            if (packet->stream_index == videoStreamIndex)
            {
                Logger::getInstance().log("Decoding a video packet: " + segment->getUri(), Logger::Severity::DEBUG, TAG);
                decodeNextFrame(packet);
            }
            else
            {
                Logger::getInstance().log("Ignoring a non video packet: " + segment->getUri(), Logger::Severity::DEBUG, TAG);
            }
        }
        else if (ret == AVERROR_EOF)
        {
            Logger::getInstance().log("End of segment reached, uri: " + segment->getUri(), Logger::Severity::DEBUG, TAG);
            if(segment->getNumFrames() == 0) {
                Logger::getInstance().log("Failed to download segment, uri: " + segment->getUri(), Logger::Severity::ERROR, TAG);
                segment->download_failed();
                break;
            }
            segment->download_complete();
            break;
        }
        else
        {
            // TODO: see what to do here exactly
            Logger::getInstance().log("Failed to demux the packet for uri: " + segment->getUri() + ", error: " + std::to_string(ret) + segment->getUri(), Logger::Severity::ERROR, TAG);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        av_packet_free(&packet);
    }
}

void Decoder::decodeNextFrame(AVPacket *packet)
{
    if (stopDecoding)
    {
        return;
    }
    // Allocate frame
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        throw std::runtime_error("Failed to allocate frame");
    }

    received_packets++;

    while (av_read_frame(formatContext, packet) >= 0)
    {
        if (packet->stream_index == videoStreamIndex)
        {
            if (avcodec_send_packet(codecContext, packet) == 0)
            {
                if (avcodec_receive_frame(codecContext, frame) == 0)
                {
                    decoded_frames++;
                    num_of_failed_frames_in_arrow = 0;
                    // outputQueue.push(frame);
                    segment->calculateStatistics(frame, get_timebase());
                    av_frame_free(&frame);
                    return;
                }
            }
            else
            {
                num_of_failed_frames_in_arrow++;
                if (num_of_failed_frames_in_arrow > 20)
                {
                    av_frame_free(&frame);
                    throw std::runtime_error("Failed to decode segment");
                }
                else
                {
                    Logger::getInstance().log("Failed to decode packet" + segment->getUri(), Logger::Severity::DEBUG, TAG);
                }
            }
        }
    }
    av_frame_free(&frame);
}

int Decoder::getWidth() const
{
    return codecContext->width;
}

int Decoder::getHeight() const
{
    return codecContext->height;
}

AVPixelFormat Decoder::getPixelFormat() const
{
    return codecContext->pix_fmt;
}

AVRational Decoder::get_timebase() const
{
    return formatContext->streams[videoStreamIndex]->time_base;
}