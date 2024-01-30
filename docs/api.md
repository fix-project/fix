# Fixpoint API: Fix's interface in Wasm

The Wasm module must export a function named `_fixpoint_apply` of type
`externref -> externref`. Fix uses the `externref` type to represent a Handle.
The function will be called with the Name of the combination. The Object named
in the function's return value will be the result of `apply`.

To give the Wasm module access to the and the ability to create new Expressions,
Fix makes available the following host functions as imports. Each of them can be
imported from the \"`fix`\" namespace.

Note: the API uses `externref`'s to represent Handles. Some Handles below have
more specific Types. We refer here to Handles with `&`. Thus, when we `&Blob`,
we mean a handle to a Blob, represented as an `externref` to the Wasm API. Trees
have an extra status of being tagged. When we write `&tagged ObjectTree`, we
mean a handle to a tagged ObjectTree. When we write `&ObjectTree`, we mean a
handle to an ObjectTree and whether it's tagged does not matter.

Inputing a handle that does not fulfill the type requirement of an api
function's input leads to undefined behavior.

## nil
```rust
fn nil() -> &Literal;
```
Returns the canonical "nil" Handle.

## attach_tree_ro_table

```rust
fn attach_tree_ro_table_N(handle: &AnyTree) -> ();
```
"attaches" the Tree to the Wasm table numbered *N*. Future calls to `table.get`,
`table.size` or `table.copy` (referring to table *N* as a source) will refer to
the entries of this Tree.

The table must:
- Be exported under the name `ro_table_N`,
- Not be exported under another name,
- Never be the target of an instruction that would mutate it (`table.set`,
  `table.init`, `table.fill`, `table.copy`, `table.grow`). These table
  properties are checked statically upfront by wasminspector.

## attach_blob_ro_mem

```rust
fn attach_blob_ro_mem_N(handle: &Blob) -> ();
```
“attaches” the Blob to the Wasm memory numbered *N*. Future calls to `*.load`
instructions as well as `memory.size` or `memory.copy` (referring to memory
*N* as a source) will refer to the bytes of this Blob.

The memory must:
- Be exported under the name `ro_mem_N`,
- Not be exported under another name,
- Never be the target of an instruction that would mutate it (`*.store`,
  `memory.init`, `memory.fill`, `memory.copy`, `memory.grow`).
  These memory properties are checked statically upfront by wasminspector.

## size_ro_mem

```rust
fn size_ro_mem_N() -> i32;
```
Returns the size of the Blob attached to memory *N*, in bytes (or zero if no
blob is attached to this memory). This differs from the `memory.size`
instruction, which must return an integer number of 64 KiB pages. The read-only
memory must meet the same statically checked requirements as above.

## create_blob_rw_mem

```rust
fn create_blob_rw_mem_N(length_in_bytes: i32) -> &Blob;
```
“Detaches” the contents of memory *N* and creates a Blob with length
in bytes as given in the i32 argument. This length must be less than or equal
to the data length of memory *N*. Returns a Handle referencing the newly
created Blob. The memory must be exported under the name `rw_mem_N`.

## create_blob

```rust
fn create_blob_i32(value: i32) -> &Literal;
fn create_blob_i64(value: i64) -> &Literal;
```
Same as `create_blob_rw_mem_N`, but creates the Blob from an argument given on
the stack instead of from a read-write memory.

The possible immediate types may be expanded in the future from i32 and i64 o be
any of the numeric or vector types of Wasm: i32, i64, f32, f64, or v128.

```rust
fn create_blob_string_ro_mem_N(index_in_memory:i32, length_in_bytes: i32) -> &Blob;
fn create_blob_string_rw_mem_N(index_in_memory:i32, length_in_bytes: i32) -> &Blob;
```
Same as `create_blob_rw_mem_N`, but creates the Blob by copying the data at
[index, index + length).

Not yet implemented.

## create_tree_rw_table

```rust
fn create_tree_rw_table_N(length_in_entries: i32) -> &ExpressionTree or &ObjectTree or &ValueTree;
```
"Detaches” the contents of table *N* and creates a Tree whose length in entries
is given by the i32 argument. This length must be less than or equal to the
length of table *N*. Returns a Handle referencing the newly created Tree. The
table must be exported under the name `rw_table_N`. The actual Tree type is
selected as the strictest Tree type that the Tree satisfies.

## create_thunk

```rust
fn create_application_thunk(handle: &AnyTree) -> &Application;
```
Given a Tree, creates a corresponding Application Thunk as a Handle. The Tree should
be in Encode format, but this property is not checked by fixpoint.

```rust
fn create_identification_thunk(handle: &Value) -> &Identification;
```
Given a Value, creates a corresponding Identification Thunk as a Handle.

Not yet implemented.

```rust
fn create_selection_thunk(handle: &ObjectTree) -> &Selection;
```
Given an ObjectTree, creates a corresponding Selection Thunk as a Handle.

Not yet implemented.

## create_tag

```rust
fn create_tag(target: &Expression, tag_data: &Expression) -> &tagged ExpressionTree or ObjectTree or ValueTree
```
Given the `target` object to be tagged and the custom `tag_data`, returns the
Handle of a tagged Tree `{current_procedure, x, y}`. The actual Tree type is
determined similarly to `create_tree`. Used to unforgeably mark Objects.

## get_length

```rust
fn get_length(handle: &AnyTree) -> i32;
fn get_length(handle: &AnyTreeRef) -> i32;
fn get_length(handle: &Blob) -> i32;
fn get_length(handle: &BlobRef) -> i32;
```
Given a Handle, returns the length (in bytes or number of
entries) of the corresponding data.

## create_encode

```rust
fn create_strict_encode(handle: &Thunk) -> &Strict;
fn create_shallow_encode(handle: &Thunk) -> &Shallow
```
Given a Thunk, returns the handle of an Encode.

## is_euqal

```rust
fn is_equal(lhs: &Value, rhs: &Value) -> bool
```
Given two Handles to Value, return whether they are equal.

## type checkers

```rust
fn is_blob(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &Blob.

```rust
fn is_tree(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &ExpressionTree or &ObjectTree or
&ValueTree.

```rust
fn is_tag(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &tagged ExpressionTree or &tagged
ObjectTree or &tagged ValueTree.

```rust
fn is_blob_ref(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &BlobRef.

```rust
fn is_tree_ref(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &ObjectTreeRef or &ValueTreeRef.

```rust
fn is_value(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &Value.

```rust
fn is_thunk(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &Thunk.

```rust
fn is_encode(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &Encode.

```rust
fn is_shallow(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &Shallow.

```rust
fn is_strict(handle: &Fix) -> bool
```
Given a Handle, return whether it's a &Strict.
