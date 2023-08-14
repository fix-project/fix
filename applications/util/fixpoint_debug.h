#pragma once

#include "fixpoint_util.h"

extern externref debug_try_lift( externref handle )
  __attribute__( ( import_module( "fixpoint" ), import_name( "debug_try_lift" ) ) );
extern externref debug_try_inspect( externref handle )
  __attribute__( ( import_module( "fixpoint" ), import_name( "debug_try_inspect" ) ) );
extern externref debug_try_evaluate( externref handle )
  __attribute__( ( import_module( "fixpoint" ), import_name( "debug_try_evaluate" ) ) );
