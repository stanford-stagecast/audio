/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#pragma once

#include "file_descriptor.hh"
#include "jpeg.hh"
#include "mmap.hh"
#include "raster.hh"

#include <linux/videodev2.h>
#include <optional>
#include <unordered_map>

class CameraFD : public FileDescriptor
{
public:
  using FileDescriptor::FileDescriptor;

  void buffer_dequeued() { register_read(); }
};

class Camera
{
private:
  static constexpr unsigned int NUM_BUFFERS = 4;
  static constexpr uint32_t pixel_format = V4L2_PIX_FMT_MJPEG;

  uint16_t width_;
  uint16_t height_;
  std::string device_name_;

  CameraFD camera_fd_;
  std::vector<MMap_Region> kernel_v4l2_buffers_;
  unsigned int next_buffer_index = 0;
  JPEGDecompresser jpegdec_ {};

  void init();

  unsigned int frame_count_ {};

public:
  Camera( const uint16_t width, const uint16_t height, const std::string& device_name );

  ~Camera();

  void get_next_frame( RasterYUV422& raster );

  FileDescriptor& fd() { return camera_fd_; }
};
