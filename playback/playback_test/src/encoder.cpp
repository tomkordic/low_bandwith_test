#include "encoder.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

Encoder::Encoder(const std::string &outputFile, int width, int height,
                 int encodeFrequency)
    : codecContext(nullptr), file(nullptr), processedFrameCounter(0),
      receivedFrameCounter(0), resizedFrames(5), swsContext(nullptr),
      state(EncoderState::LoadingEncoder), encodeFrequency(encodeFrequency),
      width(width), height(height), numberOfInputFramesToGetFirstNalu(-1) {

  initEncoder();

  // Open output file
  file = fopen(outputFile.c_str(), "wb");
  if (!file) {
    throw std::runtime_error("Failed to open output file");
  }
}

Encoder::~Encoder() {
  // Flush the encoder and write any remaining packets
  flushEncoder(FlushOption::FlushLastFrame);

  // Close and cleanup
  if (file) {
    fclose(file);
  }
  releaseEncoder();
  if (swsContext) {
    sws_freeContext(swsContext);
  }
}

void Encoder::saveCurrentFrameToFile(AVFrame *resizedFrame) {
  std::ostringstream fileName;
  fileName << "/Users/tomislavkordic/ocean_eye/image_capture/frames/frame"
           << "_" << std::setw(3) << std::setfill('0') << processedFrameCounter
           << ".yuv";
  // Open the file for writing
  std::ofstream file(fileName.str());
  if (!file) {
    std::cerr << "Error: Could not open file " << fileName.str()
              << " for writing." << std::endl;
    return;
  }

  // Write Y (luma) plane
  for (int y = 0; y < resizedFrame->height; ++y) {
    file.write(reinterpret_cast<const char *>(resizedFrame->data[0] +
                                              y * resizedFrame->linesize[0]),
               resizedFrame->width);
  }

  // Write U (chroma) plane
  for (int y = 0; y < resizedFrame->height / 2; ++y) {
    file.write(reinterpret_cast<const char *>(resizedFrame->data[1] +
                                              y * resizedFrame->linesize[1]),
               resizedFrame->width / 2);
  }

  // Write V (chroma) plane
  for (int y = 0; y < resizedFrame->height / 2; ++y) {
    file.write(reinterpret_cast<const char *>(resizedFrame->data[2] +
                                              y * resizedFrame->linesize[2]),
               resizedFrame->width / 2);
  }

  // Close the file
  file.close();
}

AVFrame *Encoder::resize(AVFrame *frame) {
  if (swsContext == nullptr) {
    swsContext =
        sws_getContext(frame->width, frame->height,
                       static_cast<AVPixelFormat>(frame->format), // Source
                       codecContext->width, codecContext->height,
                       codecContext->pix_fmt, // Destination
                       SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsContext) {
      throw std::invalid_argument(
          "Error: Could not initialize scaling context");
    }
    // init rescale images
    for (int i = 0; i < resizedFrames.getMaxSize(); i++) {
      AVFrame *resizedFrame = av_frame_alloc();
      if (av_image_alloc(resizedFrame->data, resizedFrame->linesize,
                         codecContext->width, codecContext->height,
                         codecContext->pix_fmt, 32) < 0) {
        throw std::invalid_argument(
            "Error: Could not allocate resized frame buffer");
      }
      resizedFrame->width = codecContext->width;
      resizedFrame->height = codecContext->height;
      resizedFrame->format = codecContext->pix_fmt;
      resizedFrame->pts = -1;
      resizedFrames.push(resizedFrame);
    }
  }
  AVFrame *resizedFrame = resizedFrames.pop();
  sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height,
            resizedFrame->data, resizedFrame->linesize);
  // TODO: check if this was done correctly, save these to the file
  // if (framesReceived % 900 == 0) {
  //   std::cout << "Saving frame: " << framesReceived << std::endl;
  //   saveCurrentFrameToFile(resizedFrame);
  // }
  return resizedFrame;
}

void Encoder::encodeFrame(AVFrame *frame) {
  if (!frame) {
    throw std::invalid_argument("Input frame is null");
  }
  if (state == EncoderState::ExtractingFrames &&
      receivedFrameCounter % encodeFrequency != 0) {
    receivedFrameCounter++;
    return;
  }

  receivedFrameCounter++;
  AVFrame *resizedFrame = resize(frame);
  resizedFrame->pts = processedFrameCounter++;
  std::cout << "Processing packet with pts: " << resizedFrame->pts << std::endl;

  // Send the frame to the encoder
  int result = avcodec_send_frame(codecContext, resizedFrame);
  if (result < 0) {
    throw std::runtime_error("Failed to send frame to encoder");
  }

  resizedFrames.push(resizedFrame);

  // Receive encoded packets
  AVPacket *packet = av_packet_alloc();
  packet->data = nullptr;
  packet->size = 0;

  while (avcodec_receive_packet(codecContext, packet) == 0) {
    if (state == EncoderState::LoadingEncoder) {
      state = EncoderState::ExtractingFrames;
      numberOfInputFramesToGetFirstNalu = processedFrameCounter;
    }
    fwrite(packet->data, 1, packet->size, file); // Write last packet to file
    av_packet_unref(packet);                     // Free the packet
  }
  av_packet_free(&packet);
}

void Encoder::initEncoder() {
  // Find the H.265 encoder
  const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
  if (!codec) {
    throw std::runtime_error("H.265 encoder not found");
  }

  // Allocate codec context
  codecContext = avcodec_alloc_context3(codec);
  if (!codecContext) {
    throw std::runtime_error("Failed to allocate codec context");
  }

  // Set encoding parameters
  codecContext->bit_rate = 400000; // Bitrate in bits per second
  codecContext->width = width;
  codecContext->height = height;
  codecContext->time_base = AVRational{1, 15};
  codecContext->framerate = AVRational{15, 1};
  codecContext->gop_size = 10;    // Group of pictures size
  codecContext->max_b_frames = 1; // Allow one B-frame
  codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

  // Prepare the AVDictionary for x265-specific options
  AVDictionary *codecOptions = nullptr;

  // Set x265-specific parameters
  av_dict_set(&codecOptions, "x265-params",
              "keyint=1:scenecut=0:lookahead=0:vbv-bufsize=0", 0);

  // Open the codec
  if (avcodec_open2(codecContext, codec, &codecOptions) < 0) {
    std::cerr << "Failed to open codec\n";
    avcodec_free_context(&codecContext);
    av_dict_free(&codecOptions);
    throw std::runtime_error("Failed to open codec");
  }

  // Clean up
  av_dict_free(&codecOptions);
  avcodec_free_context(&codecContext);
}

void Encoder::releaseEncoder() {
  if (codecContext) {
    avcodec_free_context(&codecContext);
  }
}

void Encoder::resetAndLoad() {
  releaseEncoder();
  initEncoder();
  // Feed with all available queued images
  int lastPts = -1;
  int loop = 0;
  while (true) {
    loop++;
    AVFrame *frame = resizedFrames.peek();
    if (frame->pts == -1) {
      // uninit frame skip
      resizedFrames.pop();
      resizedFrames.push(frame);
      continue;
    }
    if (frame->pts <= lastPts && lastPts != -1) {
      // Full circle, stop feeding
      break;
    }
    lastPts = frame->pts;
    int result = avcodec_send_frame(codecContext, frame);
    if (result < 0) {
      throw std::runtime_error("Failed to send frame to encoder");
    } else {
      std::cout << "Preloading frame with pts: " << frame->pts << std::endl;
    }
    resizedFrames.pop();
    resizedFrames.push(frame);
  }
  std::cout << "Encoder reset done !" << std::endl;
}

void Encoder::flushEncoder(FlushOption option) {
  // Send a null frame to signal the end of the stream
  if (avcodec_send_frame(codecContext, nullptr) >= 0) {
    AVPacket *packet = av_packet_alloc();
    AVPacket *last_packet = av_packet_alloc();
    packet->data = nullptr;
    packet->size = 0;

    while (avcodec_receive_packet(codecContext, packet) == 0) {
      if (option == FlushOption::FlushAllFrames) {
        fwrite(packet->data, 1, packet->size, file); // Write packet to file
      }
      if (packet->size > 0) {
        last_packet = av_packet_clone(packet);
      }
      av_packet_unref(packet); // Free the packet
    }
    if (option == FlushOption::FlushLastFrame) {
      fwrite(last_packet->data, 1, last_packet->size,
             file); // Write last packet to file
    }
    av_packet_free(&last_packet);
    av_packet_free(&packet);
  }
}