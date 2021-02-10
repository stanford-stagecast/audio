#pragma once

#include <optional>
#include <string>

struct HTTPHeaders
{
  std::optional<size_t> content_length {};
  std::string host {};
  bool connection_close {};
};

struct HTTPRequest
{
  std::string method {}, request_target {}, http_version {};
  HTTPHeaders headers {};
  std::string body {};
};

struct HTTPResponse
{
  std::string http_version {}, status_code {}, reason_phrase {};
  HTTPHeaders headers {};
  std::string body {};
};
