#pragma once

#include <variant>

#include "blob.hh"
#include "thunk.hh"
#include "tree.hh"

using Object = std::variant<Blob, Tree, Thunk>;
