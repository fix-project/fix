#pragma once

#include <optional>
#include <string>

struct HTTPHeaders
{
  std::optional<size_t> content_length {};
  std::string host {};
  std::string connection {};
  std::string content_type {};
  std::string upgrade {};
  std::string origin {};
  std::string sec_websocket_key {}, sec_websocket_accept {};
  std::string location {};
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
