#ifndef ENCODER_HPP
#define ENCODER_HPP

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include "avframequeue.hpp";

#include <cstdio>
#include <stdexcept>
#include <string>

enum EncoderState {
  LoadingEncoder,  // Accepting every input frame until encoder produces an
                   // output frame
  ExtractingFrames // Accepting one in many frames
};

enum FlushOption {
  FlushAllFrames, // Save everything to the file
  FlushLastFrame  //  save only the last encoded frame to the file
};

class Encoder {
public:
  /**
   * @brief Constructor for Encoder.
   *
   * @param outputFile Path to the output file where the encoded bitstream will
   * be saved.
   * @param width Width of the video frames to encode.
   * @param height Height of the video frames to encode.
   * @param encodeFrequency Accept one frame out of many.
   *
   * @throws std::runtime_error if initialization fails.
   */
  Encoder(const std::string &outputFile, int width, int height,
          int encodeFrequency);

  /**
   * @brief Destructor for H265Encoder.
   *
   * Frees resources and ensures the encoder is properly flushed.
   */
  ~Encoder();

  /**
   * @brief Encodes a video frame and writes it to the output file.
   *
   * @param frame A pointer to the AVFrame to encode. The frame must have a
   * format of AV_PIX_FMT_YUV420P.
   *
   * @throws std::invalid_argument if the input frame is null.
   * @throws std::runtime_error if encoding fails.
   */
  void encodeFrame(AVFrame *frame);
  /**
   * @brief Flushes the encoder to ensure all remaining packets are written to
   * the output file.
   */
  void flushEncoder(FlushOption option);

private:
  SwsContext *swsContext;
  AVFrameQueue resizedFrames;
  AVCodecContext *codecContext; ///< Pointer to the FFmpeg codec context.
  FILE *file;                   ///< Pointer to the output file.
  int processedFrameCounter; ///< Counter for frame presentation timestamps (PTS).
  int receivedFrameCounter;
  int encodeFrequency;
  int width;
  int height;
  int numberOfInputFramesToGetFirstNalu;
  EncoderState state;

  void saveCurrentFrameToFile(AVFrame *resizedFrame);
  AVFrame *resize(AVFrame *frame);
  void initEncoder();
  void releaseEncoder();
  void resetAndLoad();
};

#endif // ENCODER_HPP
