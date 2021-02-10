#pragma once

#include "http_structures.hh"
#include "ring_buffer.hh"

class HTTPRequestWriter
{
  HTTPRequest request_;

  size_t bytes_written_ { false };
  bool finished_ { false };

  class WriteAttempt
  {
    RingBuffer& buffer_;
    size_t& bytes_written_;
    size_t remaining_offset_;
    bool completed_ { true };

  public:
    WriteAttempt( RingBuffer& buffer, size_t& bytes_written_ );

    void write( std::string_view str );
    bool completed() const { return completed_; }
  };

public:
  HTTPRequestWriter( HTTPRequest&& request );
  void write_to( RingBuffer& buffer );

  bool finished() const { return finished_; }
};
