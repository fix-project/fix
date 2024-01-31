#pragma once

#include "handle.hh"
#include "interface.hh"
#include "spans.hh"

Handle<Fix> parse_args( IRuntime& rt, span_view<char*>& args );
void parser_usage_message();
