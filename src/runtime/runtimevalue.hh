#pragma once

#include "name.hh"
#include "tree.hh"

#include "objectreference.hh"
#include "mutablevalue.hh"

using RuntimeReference = std::variant<ObjectReference, MutableValueReference>;
