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
  |  |  ├─ handle to TRUSTED_COMPILE
  |  |  ├─ blob:EXECUTABLE
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

# Minimum Repository

For any given evaluation, there's a minimum set of information which must be
present in a Fix repo to perform it.  We call this the "minimum repository", or
"minrepo", of the Handle to be evaluated.

The minimum repository is defined recursively:

1. The minrepo of a lazy Handle is that lazy Handle.
2. The minrepo of a Blob is its Handle and contents.
3. The minrepo of a Strict Tree/Tag is the union of the minrepos of every
   child, as well as the Handle and contents of the Tree/Tag itself.
4. The minrepo of a Shallow Tree/Tag is the Handle and contents of the
   Tree/Tag.
5. The minrepo of a Thunk is the union of the minrepo of the ENCODE Tree and
   the Thunk's Handle.

However, we can elide all Handles except the root from the minimum repository,
since every child must be reachable from the root; the Handles are therefore
contained within Trees or Tags already.  This reduces the minrepo to the
contents of Blobs, Tags, and Trees which are reachable using the above rules.
The most notable changes are:
1. Lazy Handles can be completely ignored, since they carry no data.
2. Thunk Handles can be skipped, since all their data are carried on the
   ENCODE.

When Fixpoint sends a value to another node, it must ensure the minrepo is
available on the remote node.  A Fixpoint Handle for which the minrepo is not
present is not a valid Fix Handle, since attaching it is unsafe and may cause a
panic.
