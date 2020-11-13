#pragma once

#include <array>
#include <cstdint>

#include "spans.hh"

struct Packet
{
  uint8_t size_1, size_2;
  std::array<char, 50> frame1, frame2;
  string_span f1() { return { frame1.data(), frame1.size() }; }
  string_span f2() { return { frame2.data(), frame2.size() }; }
  operator std::string_view() const { return { reinterpret_cast<const char*>( this ), sizeof( Packet ) }; }
};
