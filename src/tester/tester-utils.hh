#pragma once

#include "handle.hh"
#include "interface.hh"

Handle<Fix> parse_args( IRuntime& rt, std::span<char*>& args );
void parser_usage_message();
