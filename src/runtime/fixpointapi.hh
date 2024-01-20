#pragma once

#include "handle.hh" // IWYU pragma: keep
#include "runtimestorage.hh"
#include "wasm-rt.h"

namespace fixpoint {
inline thread_local RuntimeStorage* storage;
inline thread_local Handle<Fix> current_procedure;

// Returns the canonical "nil" Handle (the Handle with all zero bits, which is a zero-length Literal).
u8x32 nil( void );

// Traps if handle is not Handle<AnyTree>
void attach_tree( u8x32 handle, wasm_rt_externref_table_t* target_memory );

// Traps if handle is not Handle<Blob> (Handle<Named> or Handle<Literal>)
void attach_blob( u8x32 handle, wasm_rt_memory_t* target_memory );

// Return Handle<AnyTree>
u8x32 create_tree( wasm_rt_externref_table_t* table, size_t size );

// Return Handle<Blob>
u8x32 create_blob( wasm_rt_memory_t* memory, size_t size );

// Return a tagged Handle<ValueTree> or Handle<ObjectTree> or Handle<ExpressionTree>
u8x32 create_tag( u8x32 handle, u8x32 type );

// Return Handle<Blob>
u8x32 create_blob_i32( uint32_t content );

// Return Handle<Blob>
u8x32 create_blob_i64( uint64_t content );

// Return Handle<Blob>
u8x32 create_blob_string( uint32_t index, uint32_t length, wasm_rt_memory_t* memory );

// Return Handle<Application>, traps if handle is not Handle<ExpressionTree>
u8x32 create_application_thunk( u8x32 handle );

// Return Handle<Identification>, traps if handle is not Handle<Value>
u8x32 create_identity_thunk( u8x32 handle );

// Return Handle<Selection>, traps if handle is not Handle<ObjectTree>
u8x32 create_selection_thunk( u8x32 handle, uint32_t idx );

// Traps if handle is not Handle<AnyTree> or Handle<AnyTreeRef> or Handle<Blob> or Handle<BlobRef>
uint32_t get_length( u8x32 handle );

// Return Handle<Strict>, traps if handle is not Handle<Thunk>
u8x32 create_strict_encode( u8x32 handle );

// Return Handle<Shallow>, traps if handle is not Handle<Thunk>
u8x32 create_shallow_encode( u8x32 handle );

// Traps if lhs or rhs is not Handle<Value>
uint32_t is_equal( u8x32 lhs, u8x32 rhs );

// XXX
uint32_t is_blob( u8x32 handle );
uint32_t is_tree( u8x32 handle );
uint32_t is_tag( u8x32 handle );
uint32_t is_blob_ref( u8x32 handle );
uint32_t is_tree_ref( u8x32 handle );
uint32_t is_value( u8x32 handle );
uint32_t is_graph( u8x32 handle );
uint32_t is_thunk( u8x32 handle );
uint32_t is_encode( u8x32 handle );
uint32_t is_shallow( u8x32 handle );
uint32_t is_strict( u8x32 handle );

// Unsafe_io prints the contents mem[str_index: index + length]. Traps if
// index + length - 1 is out of bounds.
void unsafe_io( int32_t index, int32_t length, wasm_rt_memory_t* memory );

uint32_t pin( u8x32 src, u8x32 dst );
}

namespace fixpoint_debug {
// Attempt to lift the current handle to a stronger accessibility (nondeterministic)
u8x32 try_lift( u8x32 handle );

// Attempt to get the Encode from a Handle to a Thunk (nondeterministic if we add GC)
u8x32 try_inspect( u8x32 handle );

// Attempt to get the evaluated version of a Handle (nondeterministic)
u8x32 try_evaluate( u8x32 handle );
}
