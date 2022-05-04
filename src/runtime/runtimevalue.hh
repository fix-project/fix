#pragma once

#include "name.hh"
#include "tree.hh"

#include "mutablevalue.hh"
#include "objectreference.hh"

using RuntimeReference = std::variant<ObjectReference, MutableValueReference>;
