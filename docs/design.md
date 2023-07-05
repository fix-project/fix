# Fix
## Objects
The basic type is an Object. Each Object is immutable and has one canonical
representations, and is either:

* A Value, which is either:
  - Blob: a vector of bytes
  - Tree: a vector of Handles
  - Tag: a tuple of Handles

* or a Thunk, which represents a Value by specifying a way to compute it.
  Internally, a Thunk contains a Tree (or a strict Handle to a Tree) in Encode
  format.

## Handles
A Handle is an opaque identifier. It idenifies a particular Object and
specifies an accessibility: whether it is "strict" or "shallow" or "lazy"[^1].
Two Handles that identify the same Object with the same accessibility are
equivalent and indistinguishable.

## Fully-evaluated Values:
  * A Blob is always fully-evaluated
  * A Tree is fully-evaluated if and only if any entry reachable through a
  (strict)*(shallow)? path is not a Thunk. 
  * A Tag is fully-evaluated if and only if the first entry is fully-evaluated.

[^1]: These accessibility status are defined such that
  each Encode can specify which part of the Encode Tree is actually needed for
  the current computation. "strict" is saying value is needed. "lazy" is saying
  the value is not needed. "shallow" is saying the "top-level" of the value is
  needed, and therefore a Tree would not contain any shallow Thunk after it is
  fully evaluated.

## Encodes
  An Encode is a Tree in a particular format. It describes the application of a
  function to inputs, producing an Object as output. The first entry specifies
  the metadata, possibly including resource limits at runtime (e.g. maximum 
  pages of mutable memory), and the format of the Encode. An Encode's format 
  is either "apply" or "lift" (not yet implemented).

  An "apply" Encode may nest another Encode within itself, thus allowing for
  nested argument capture. 

  For an "apply" Encode, the second entry in the tree specifies the procedure,
  which is either:
    1. A Tag that the first entry is a Blob, the second entry is a trusted 
    compilation toolchain, and the third entry is "Runnable" or
    2. An Encode

  For a "lift" Encode, the second entry is the Handle to the Object to be lifted.

  ```
  "apply" Encode example
  tree:5
  ├─ string:unused
  ├─ tree:4
  |  ├─ string:unused
  |  ├─ tag
  |  |  ├─ blob:EXECUTABLE
  |  |  ├─ handle to TRUSTED_COMPILE
  |  |  ├─ literal blob "Runnable"
  |  ├─ ARGUMENT_2_1
  |  ├─ ARGUMENT_2_2
  ├─ ARGUMENT_1_1
  ├─ ARGUMENT_1_2
  ├─ ARGUMENT_1_3
  ```

## Tags:
A Tag always contains exactly three entries

  0. An entry of any type (Tree, Thunk, Blob, Encode)
  1. The signer: the name of an ELF blob or the trusted compilation toolchain
  2. The metadata: a blob of custom data (the compiler uses "Runnable")

A procedure can create a Tag where the first entry can be anything (what 
the procedure wishes to tag), the second entry is the Name of the ELF 
blob of the procedure, and the third entry is a Blob (the metadata). 

# Operations: eval, apply and lift
## Eval
`eval` transforms Objects to either strict fully-evaluated Values, or shallow
Values, or lazy Objects. It takes the Handle of an Object and returns the
Handle of the resulting Value. For a Handle *x*, `eval(`*x*`)` is defined as:
  * If *x* is a Blob, or a shallow/lazy Tree/Tag, or a lazy Thunk, return *x*
  * If *x* is a strict Tree:
    For each entry *y*, replace with `eval(`*y*`)`.
  * If *x* is a strict Tag:
    Replace the first entry *y* with `eval(`*y*`)`.
  * If *x* is a strict or shallow Thunk,
    1. Let *y* be the Encode that *x* refers to. Let *z* be `eval(`*y*`)`
    2. If *z* is not in Encode format, traps
    3. If *z* is an apply-Encode,
    - if *x* is strict, return `eval(apply(`*z*`))`.
    - if *x* is shallow, return `eval(make_shallow(apply(`*z*`)))`.
    4. If *z* is a lift-Encode, let *m* be the second entry of *z*
    - if *x* is strict, return `lift_strict(`*m*`)`.
    - if *x* is shallow, return `lift_shallow(`*m*`)`

## make\_shallow
`make_shallow(x)`: if x is a strict Tree or Tag, return a shallow Tree or Tag that
has the same content as x, else return x.

## Apply
`apply` transforms Trees (in apply-Encode format) to Handles. For a Tree *x* in
Encode format, `apply(`*x*`)` is defined as:

1. call the ELF's apply function, passing it x and providing host functions
   that let it:
   - access any Value reachable through a (strict)<sup>0 or more</sup>(shallow)<sup>0 or
   1</sup> path
   - create new Objects
   - return an Object as its output

2. The return value from the ELF function is the value of `apply(`*x*`)`
3. If *x* is not in apply-Encode format, or executing the ELF function traps,
   `apply` fails.

## Lift\_strict
`lift_strict` transforms non-strict Handles to strict Handles. For a Handle
*x*, `lift_strict` is defined as:
  * If *x* is lazy, returns a strict Handle that refers to the same Object
    that *x* refers to. If *x* is shallow, returns a strict Handle that refers
    to the same Object that *x* refers to.

## Lift\_shallow
`lift_shallow` transforms lazy Handles to shallow Handles. For a Handle
*x*, `lift_shallow` is defined as:
  * If *x* is lazy, returns a shallow Handle that refers to the same Object
    that *x* refers to. Otherwise, return *x*.

# Fix's interface to Wasm
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
Handle of a Tag `{x, current_procedure, y}`. Used to unforgeably mark Objects.

Currently, `tag_data` is not being checked to be a `Blob` and the strict
requirement is not known to be necessary.

## value_type

```rust
fn value_type(handle: &Object) -> i32;
fn value_type(handle: &shallow Object) -> i32;
```
Retrieve type (Tree = 0, Thunk = 1, Blob = 2 or Tag = 3) from a strict or 
shallow Handle. 

## get_length

```rust
fn get_length(handle: &Object) -> i32;
fn get_length(handle: &shallow Object) -> i32;
```
Given an strict or shallow Handle, returns the length (in bytes or number of 
entries) of the corresponding Value.

## get_total_size

```rust
fn get_total_size(handle: &any Object) -> i32;
```
Given a Handle, returns the total downstream size (in bytes) required to
transfer this Handle (with the specified accessibility) to another machine.

## access

```rust
fn access(handle: &any Object) -> i32;
```
Retrieve accessibility type (Strict = 0, Shallow = 1, Lazy = 2) from a Handle.

Not yet implemented. 

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

