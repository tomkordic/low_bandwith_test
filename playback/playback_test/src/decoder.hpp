#ifndef DECODER_HPP
#define DECODER_HPP

#include "hls_segment.hpp"

// FFmpeg headers
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
}

#include <string>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <atomic>

#include "queue.hpp"

namespace playback
{

    class Decoder
    {
    public:
        /**
         * @brief Constructor for Decoder.
         *
         * @param segment The HLS segment to decode.
         *
         * @throws std::runtime_error if initialization fails.
         */
        explicit Decoder(std::shared_ptr<HLSSegment> segment);

        /**
         * @brief Destructor for Decoder.
         *
         * Frees resources used by the decoder.
         */
        ~Decoder();

        /**
         * @brief Gets the width of the video.
         *
         * @return The width of the video in pixels.
         */
        int getWidth() const;

        /**
         * @brief Gets the height of the video.
         *
         * @return The height of the video in pixels.
         */
        int getHeight() const;

        /**
         * @brief Gets the pixel format of the video.
         *
         * @return The pixel format of the video.
         */
        AVPixelFormat getPixelFormat() const;

        AVFrame *getFrame(long timeout_ms);

        AVRational get_timebase() const;

        bool isDecoding();

        /**
         * @brief Retrieves a decoded frame from the output queue.
         *
         * @param timeout_ms The maximum time to wait in milliseconds.
         * @return A pointer to the decoded AVFrame, or nullptr if the timeout is reached.
         */
        // AVFrame *getFrame(long timeout_ms);

    public:
        int received_packets;
        int decoded_frames;
        long started_at;

    private:
        void decodingThread(); // Worker thread for decoding

        /**
         * @brief Decodes the next video frame from the input file.
         *
         * @param paclket The packet to decode.
         *
         * @return true if a frame was successfully decoded, false if end of file is reached.
         *
         * @throws std::runtime_error if decoding fails.
         */
        void decodeNextFrame(AVPacket *packet);

    private:
        std::shared_ptr<HLSSegment> segment; ///< HLS segment to decode.
        AVFormatContext *formatContext;      ///< FFmpeg format context.
        AVCodecContext *codecContext;        ///< FFmpeg codec context.
        int videoStreamIndex;                ///< Index of the video stream.
        int num_of_failed_frames_in_arrow;

        // Thread-safe queue for packets
        std::atomic<bool> stopDecoding;
        Queue<AVFrame *> outputQueue;

        // Worker thread
        std::thread decodingWorker;
    };

#endif // DECODER_HPP
}