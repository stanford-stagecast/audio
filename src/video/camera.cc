/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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

#include "camera.hh"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <iostream>

#include "exception.hh"
#include "jpeg.hh"

using namespace std;

static constexpr int capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

Camera::Camera( const uint16_t width, const uint16_t height, const string& device_name )
  : width_( width )
  , height_( height )
  , device_name_( device_name )
  , camera_fd_( CheckSystemCall( "open camera", open( device_name.c_str(), O_RDWR ) ) )
  , kernel_v4l2_buffers_()
{
  camera_fd_.set_blocking( false );

  v4l2_capability cap {};
  CheckSystemCall( "ioctl", ioctl( camera_fd_.fd_num(), VIDIOC_QUERYCAP, &cap ) );

  if ( not( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ) ) {
    throw runtime_error( "this device does not handle video capture" );
  }

  /* setting the output format and size */
  v4l2_format format {};
  format.type = capture_type;
  format.fmt.pix.pixelformat = pixel_format;
  format.fmt.pix.width = width_;
  format.fmt.pix.height = height_;

  CheckSystemCall( "setting format", ioctl( camera_fd_.fd_num(), VIDIOC_S_FMT, &format ) );

  if ( format.fmt.pix.pixelformat != pixel_format or format.fmt.pix.width != width_
       or format.fmt.pix.height != height_ ) {
    throw runtime_error( "couldn't configure the camera with the given format" );
  }

  /* setting capture parameters */
  v4l2_streamparm params {};
  params.type = capture_type;
  CheckSystemCall( "getting capture params", ioctl( camera_fd_.fd_num(), VIDIOC_G_PARM, &params ) );
  if ( params.type != capture_type ) {
    throw runtime_error( "bad v4l2_streamparm" );
  }
  if ( not( params.parm.capture.capability & V4L2_CAP_TIMEPERFRAME ) ) {
    throw runtime_error( "can't set frame rate" );
  }
  params.parm.capture.timeperframe.numerator = 1;
  params.parm.capture.timeperframe.denominator = 24;
  CheckSystemCall( "setting capture params", ioctl( camera_fd_.fd_num(), VIDIOC_S_PARM, &params ) );
  if ( params.parm.capture.timeperframe.numerator != 1 or params.parm.capture.timeperframe.denominator != 24 ) {
    throw runtime_error( "can't set frame rate to 24 fps" );
  }

  init();
}

void Camera::init()
{
  kernel_v4l2_buffers_.clear();

  /* tell the v4l2 about our buffers */
  v4l2_requestbuffers buf_request {};
  buf_request.type = capture_type;
  buf_request.memory = V4L2_MEMORY_MMAP;
  buf_request.count = NUM_BUFFERS;

  CheckSystemCall( "buffer request", ioctl( camera_fd_.fd_num(), VIDIOC_REQBUFS, &buf_request ) );

  if ( buf_request.count != NUM_BUFFERS ) {
    throw runtime_error( "couldn't get enough video4linux2 buffers" );
  }

  /* allocate buffers */
  for ( unsigned int i = 0; i < NUM_BUFFERS; i++ ) {
    v4l2_buffer buffer_info;
    buffer_info.type = capture_type;
    buffer_info.memory = V4L2_MEMORY_MMAP;
    buffer_info.index = i;

    CheckSystemCall( "allocate buffer", ioctl( camera_fd_.fd_num(), VIDIOC_QUERYBUF, &buffer_info ) );

    kernel_v4l2_buffers_.emplace_back(
      nullptr, buffer_info.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera_fd_.fd_num(), buffer_info.m.offset );

    CheckSystemCall( "enqueue buffer", ioctl( camera_fd_.fd_num(), VIDIOC_QBUF, &buffer_info ) );
  }

  CheckSystemCall( "stream on", ioctl( camera_fd_.fd_num(), VIDIOC_STREAMON, &capture_type ) );

  frame_count_ = 0;
}

Camera::~Camera()
{
  CheckSystemCall( "stream off", ioctl( camera_fd_.fd_num(), VIDIOC_STREAMOFF, &capture_type ) );
}

void Camera::get_next_frame( RasterYUV422& raster )
{
  if ( raster.width() != width_ or raster.height() != height_ ) {
    throw runtime_error( "Camera::get_next_frame: mismatched raster size" );
  }

  v4l2_buffer buffer_info;
  buffer_info.type = capture_type;
  buffer_info.memory = V4L2_MEMORY_MMAP;
  buffer_info.index = next_buffer_index;
  buffer_info.bytesused = 0;

  CheckSystemCall( "dequeue buffer", ioctl( camera_fd_.fd_num(), VIDIOC_DQBUF, &buffer_info ) );
  camera_fd_.buffer_dequeued();

  if ( buffer_info.bytesused > 32 and not( buffer_info.flags & V4L2_BUF_FLAG_ERROR ) ) {
    const MMap_Region& mmap_region = kernel_v4l2_buffers_.at( next_buffer_index );

    if ( frame_count_ > 5 ) {
      try {
        jpegdec_.begin_decoding( static_cast<string_view>( mmap_region ).substr( 0, buffer_info.bytesused ) );
        jpegdec_.decode( raster );
      } catch ( const exception& e ) {
        cerr << "JPEG exception in Camera::get_next_frame(): " << e.what() << "\n";
      }
    }

    frame_count_++;
  }

  CheckSystemCall( "enqueue buffer", ioctl( camera_fd_.fd_num(), VIDIOC_QBUF, &buffer_info ) );

  next_buffer_index = ( next_buffer_index + 1 ) % NUM_BUFFERS;

  if ( jpegdec_.bad() ) {
    cerr << "Restarting Camera for " << device_name_ << "... ";
    CheckSystemCall( "stream off", ioctl( camera_fd_.fd_num(), VIDIOC_STREAMOFF, &capture_type ) );
    jpegdec_.reset();
    init();
    cerr << "done.\n";
  }
}
