# Fixpoint API: Fix's interface in Wasm

The Wasm module must export a function named `_fixpoint_apply` of type
`externref -> externref`. Fix uses the `externref` type to represent a Handle.
The function will be called with the Name of the Encode. The Object named in the
function's return value will be the result of `apply`.

To give the Wasm module access to the Encode and the ability to create new
Objects, Fix makes available the following host functions as imports. Each of
them can be imported from the \"`fix`\" namespace. 

Note: the API uses `externref`'s to represent Handles for Objects. Some Handles 
below have more specific accessibilities and specific Objects. We refer here to 
strict Handles with `&`, shallow Handles with `&shallow`, and lazy Handles with 
`&lazy`. Handles with irrelevant accessibility will be described with `&any`.
We refer to Objects as `Object` in the general case and `Tree`, `Thunk`, 
`Blob`, or `Tag` in the other cases. Thus, when we write `&Tree`, we mean a 
strict handle to a Tree, represented as an `externref` to the Wasm API, or when 
we write `&lazy Object`, we mean a lazy handle to some Object. 

## attach_tree_ro_table

```rust
fn attach_tree_ro_table_N(handle: &Tree) -> ();
fn attach_tree_ro_table_N(handle: &Tag) -> ();
```
"attaches" the Tree or Tag to the Wasm table numbered *N*. Future calls to `table.get`, 
`table.size` or `table.copy` (referring to table *N* as a source) will refer to 
the entries of this Tree or Tag.

The table must:
- Be exported under the name `ro_table_N`,
- Not be exported under another name,
- Never be the target of an instruction that would mutate it (`table.set`, 
`table.init`, `table.fill`, `table.copy`, `table.grow`).
These table properties are checked statically upfront by wasminspector.

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
to the data length of memory *N*. Returns a strict Handle referencing the newly 
created Blob. The memory must be exported under the name `rw_mem_N`.

## create_blob

```rust
fn create_blob_i32(value: i32) -> &Blob;
```
Same as `create_blob_rw_mem_N`, but creates the Blob from an argument given on the stack instead
of from a read-write memory. 

The possible immediate types may be expanded in the future from i32 to be any 
of the numeric or vector types of Wasm: i32, i64, f32, f64, or v128.

## create_tree_rw_table

```rust
fn create_tree_rw_table_N(length_in_entries: i32) -> &Tree;
```
"Detaches” the contents of table *N* and creates a Tree whose length in entries
is given by the i32 argument. This length must be less than or equal to the 
length of table *N*. Returns a strict Handle referencing the newly created Tree. 
The table must be exported under the name `rw_table_N`.

## create_thunk

```rust
fn create_thunk(handle: &Tree) -> &Thunk;
```
Given a Tree, creates a corresponding Thunk as a strict Handle. The Tree should
be in Encode format, but this property is not checked by fixpoint. 

## create_tag

```rust
fn create_tag(target: &any Object, tag_data: &Blob) -> &Tag;
```
Given the `target` object to be tagged and the custom `tag_data`, returns the
Handle of a Tag `{current_procedure, x, y}`. Used to unforgeably mark Objects.

Currently, `tag_data` is not being checked to be a `Blob` and the strict
requirement is not known to be necessary.

## get_value_type

```rust
fn get_value_type(handle: &any Object) -> i32;
```
Retrieve type (Tree = 0, Thunk = 1, Blob = 2 or Tag = 3) from a Handle. 

## get_length

```rust
fn get_length(handle: &Object) -> i32;
fn get_length(handle: &shallow Object) -> i32;
```
Given an strict or shallow Handle, returns the length (in bytes or number of 
entries) of the corresponding Value.

## get_access

```rust
fn get_access(handle: &any Object) -> i32;
```
Retrieve accessibility type (Strict = 0, Shallow = 1, Lazy = 2) from a Handle.

## shallow_get

```rust
fn shallow_get(handle: &shallow Tree, index: i32) -> &lazy Object;
```
Given a shallow Handle that refers to a Tree, returns a lazy Handle that refers
to the same Object as the Handle in the specified entry of the Tree.

## lift

```rust
fn lift(handle: &any Object) -> &strict Thunk;
```
Given a Handle, returns a Thunk that applies *fetch* to the Handle as a strict 
Handle.

Not yet implemented. 

## lower

```rust
fn lower(handle: &strict Object) -> &shallow Object;
fn lower(handle: &shallow Object) -> &lazy Object;
fn lower(handle: &lazy Object) -> &lazy Object;
```
Given a Handle, if the Handle is strict, returns the shallow Handle that points
to the same Object. If the Handle is shallow, returns the lazy Handle that 
points to the same Object. If the Handle is lazy, returns the same Handle.

Not yet implemented. 

## get_name

```rust
fn get_name(handle: &strict Object) -> (i64, i64, i64, i64);
```

Given a strict or shallow Blob Handle, return the canonical name of the Blob in 
the form of `(i64, i64, i64, i64)`. 

Not yet implemented. 

# Debugging Functions

A few additional API functions are provided for use from non-deterministic
contexts, e.g., a debug-mode thunk.  These functions query state outside of
Fix, such as whether a particular computation has already happened, but cannot
modify any state.  While it is possible to call them in deterministic contexts,
the result will always be a deterministic trap.

These functions are all partially implemented; they currently don't trap from
deterministic contexts.


## debug_try_lift

```rust
fn debug_try_lift(handle: &any Object) -> &any Object;
```

Given a Handle, attempts to return the strictest possible Handle the system can
validly construct for the same object.  The accessibility of the returned
Handle is guaranteed to be *at least* as strict as the input Handle.

## debug_try_inspect

```rust
fn debug_try_inspect(handle: &any Thunk) -> Result<&lazy Tree, &any Thunk>;
```

Given a Handle to a Thunk, attempt to return a handle to its encode.  It may
not always be possible to recover the encode from a Thunk (e.g., if the encode
was garbage collected), so this operation may fail by returning the original
Thunk.

## debug_try_evaluate

```rust
fn debug_try_evaluate(handle: &any Object) -> Result<&lazy Object, &any Object>;
```

Given a Handle, attempt to evaluate it.  This will query Fixcache, returning a
lazy Handle to the evaluated result if it's found or the original Handle if
it's not found.
