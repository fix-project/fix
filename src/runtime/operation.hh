#pragma once

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <utility>

enum class Operation : uint8_t
{
  Apply,
  Eval,
};
