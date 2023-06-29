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
specifies a capability: whether it is "strict" or "shallow" or "lazy"[^1].
Two Handles that identify the same Object with the same capability are
equivalent and indistinguishable.

## Fully-evaluated Values:
  * A Blob is always fully-evaluated
  * A Tree is fully-evaluated if and only if any entry reachable through a
  (strict)<sup>\*</sup>(shallow)<sup>0 or 1</sup> path is not a Thunk.
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
  the resoure limits at runtime (e.g. maximum pages of mutable memory) and the
  format of the Encode.
  * If the Encode format is "apply", the second entry specifies the procedure,
  which is either:
    1. A Tag that the first entry is a Blob, the second entry is "Runnable" and
    the third entry is a trusted compilation toolchain, or
    2. An Encode

  * If the Encode format is "lift", the second entry is the Handle to the
  Object to be lifted.

## Tags:
A Tag contains three entries, and can be created in two ways:

  * A procedure can create a Tag where the first entry can be anything, the second
  entry is a Blob, and the third entry is the Name of the ELF blob of the procedure.

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
    1. Let *y* be the Encode that *x* refers to. Let *z* be `eval(y)`
    2. If *z* is not in Encode format, traps
    3. If *z* is an apply-Encode,
    - if *x* is strict, return `eval(apply(*z*))`.
    - if *x* is shallow, return `eval(make_shallow(apply(`*z*`)))`.
    4. If *z* is a lift-Encode, let *m* be the second entry of *z*
    * if *x* is strict, return `lift_strict(`*m*`)`.
    * if *x* is shallow, return `lift_shallow(`*m*`)`

## make\_shallow
`make_shallow(x)`: if x is a strict Tree or Tag, return a shallow Tree or Tag that
has the same content as x, else return x.

## Apply
`apply` transforms Trees (in apply-Encode format) to Handles. For a Tree *x* in
Encode format, `apply(`*x*`)` is defined as:

1. call the ELF's apply function, passing it x and providing host functions
   that let it:
   - access any Value reachable through a (strict)<sup>\*</sup>(shallow)<sup>0 or
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
them can be imported from the \"`fix`\" namespace:

  * `attach_tree_ro_table_N` (`externref -> []`): This function "attaches" the
    Tree given in the argument to the Wasm table
    numbered *N*. Future calls to
    `table.get`, `table.size` or `table.copy` (referring to table *N* as a
    source) will refer to the entries of this Tree. The argument must be a
    strict Handle. The table must:
	- Be exported under the name `ro_table_N`,
	- Not be exported under another name,
	- Never be the target of an instruction that would mutate it (`table.set`,
    `table.init`, `table.fill`, `table.copy`, `table.grow`).*
	These table properties can be checked statically upfront.

  * `attach_blob_ro_mem_N` (`externref → []`): The function “attaches” the Blob
     given in the argument to the Wasm memory
    numbered *N*. Future calls to
    `*.load` instructions as well as `memory.size` or `memory.copy` (referring
    to memory *N* as a source) will refer to the bytes of this Blob. The
    argument must describe an strict Handle. The memory must: *
	- Be exported under the name `ro_mem_N`,
	- Not be exported under another name,
	- Never be the target of an instruction that would mutate it (`*.store`,
    `memory.init`, `memory.fill`, `memory.copy`, `memory.grow`).
  These memory properties can be checked statically upfront.

  * `attach_tag_ro_table_N` (`externref -> []`): This function "attaches" the
    the Tag given in the argument to the Wasm table
    numbered *N*. Future calls to
    `table.get`, `table.size` or `table.copy` (referring to table *N* as a
    source) will refer to the entries of this Tree. The argument must be a
    strict Handle. The table must:
	- Be exported under the name `ro_table_N`,
	- Not be exported under another name,
	- Never be the target of an instruction that would mutate it (`table.set`,
    `table.init`, `table.fill`, `table.copy`, `table.grow`).*
	These table properties can be checked statically upfront.

  * `size_ro_mem_N` (`externref -> i32`): Returns the size of the Blob attached
    to memory *N* , in bytes (or zero). This differs from the `memory.size`
    instruction, which must return an integer number of 64 KiB pages. The
    read-only memory must meet the same statically checked requirements as
    above.

  * `create_blob_rw_mem_N` (`i32 -> externref`): The function “detaches” the
    contents of memory N and creates a Blob whose length in bytes is given in
    the i32 argument. This length must be less than or equal to the data length
    of memory *N* (an integral number of pages). Returns a strict Handle naming
    the newly created Blob. The memory must be exported under the name
    `rw_mem_N`.

  * `create_blob_type` (`type -> externref`): Same as above, but creates the
    Blob from an argument given on the stack instead of from a read-write
    memory. The type can be any of the numeric or vector types of Wasm: i32,
    i64, f32, f64, or v128.

  * `create_tree_rw_table_N` (`i32 -> externref`): The function “detaches” the
    contents of table *N* and creates a Tree whose length in entries is given in
    the i32 argument. This length must be less than or equal to the length of
    table *N*. Returns a strict Handle naming the newly created Tree. The table
    must be exported under the name `rw_table_N`.

  * `create_thunk` (`externref -> externref`): Given a Tree, returns a
    corresponding Thunk as a strict Handle.

  * `create_tag` (`externref -> externref -> externref`): Given Handle x and
    Handle y, return the Handle of a Tag \{x, y, procedure\}.

  * `value_type` (`externref -> i32`): Retrieve type (Blob or Tree) from
    a strict or shallow Handle. If the Handle is a Blob or Tree, return the
    corresponding type. If the Handle is a Tag, return the type of the first
    entry of the Tag.

  * `length` (`externref -> i32`): Given an strict or shallow Handle, returns
    the length (in bytes or number of entries) of the corresponding Value.

  * `access` (`externref -> i32`): Retrieve accessibility type from a Handle.

  * `shallow_get` (`externref -> i32 -> externref`): Given a shallow Handle
    that refers to a Tree, returns a lazy Handle that refers to the same Object as
    the Handle in the specified entry of the Tree.

  * `lift` (`externref -> externref`): Given a Handle, returns a Thunk that
    applies *fetch* to the Handle as a strict Handle.

  * `sink` (`externref -> externref`): Given a Handle, if the Handle is strict,
    returns the shallow Handle that points to the same Object. If the Handle is
    shallow, returns the lazy Handle that points to the same Object. If the
    Handle is lazy, returns the same Handle.

  * `get_name` (`externref -> (i64, i64, i64, i64)`): Given a strict or shallow
    Blob Handle, return the canonical name of the Blob in the form of
    `(i64, i64, i64, i64)`
