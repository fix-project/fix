{-# OPTIONS_GHC -fwarn-incomplete-patterns #-}

-- | The Fix language (types and evaluation rules)
module Fix where

import Data.Word (Word64)

-- * Fix Types

-- | A Name is some opaque identifier, in this case a 256-bit number.
newtype Name a = Name (Word64, Word64, Word64, Word64)

-- | A Blob is a collection of bytes.
type Blob = [Char]

-- | A Tree is a collection of Names of a particular type.
data Tree a = Tree [Name a] | Tag [Name a]

-- | A Ref is a reference to an object.
type Ref a = Name a

-- | A Value is a datum which has been fully computed.  It is safe to compare Values for equality.  Values may provide either the full contents of a datum or just the name of a datum.
data Value = ValueTree ValueTree | ValueTreeRef (Ref ValueTree) | Blob Blob | BlobRef (Ref Blob)
-- | A Tree of Values.
type ValueTree = Tree Value

-- | An Object is a Value which may or may not have been computed yet.  Uncomputed data are represented as Thunks.
data Object = Thunk Thunk | Value Value | ObjectTree ObjectTree | ObjectTreeRef (Ref ObjectTree)
-- | A Thunk is a Value which has yet to be evaluated.  It is either described as an Application (of a function to arguments), an Identification (of an already-computed Value), or a Selection (of a particular element or subrange of a large structure).  It is better to use Identification or Selection Thunks where possible than applying an equivalent function, as these special Thunks have a smaller data footprint.
data Thunk = Application (Name ExpressionTree) | Identification (Name Value) | Selection (Name ObjectTree)
-- | A Tree of Objects.
type ObjectTree = Tree Object

-- | An Expression is a computation which may be further evaluated.  When evaluated, it will produce an Object.
data Expression = Object Object | Encode Encode | ExpressionTree ExpressionTree
-- | An Encode is an instruction requesting that a Thunk be replaced by the result of the function it represents.  A Strict Encode requests the complete expansion of the result, while a Shallow Encode requests partial (i.e., lazy) expansion.
data Encode = Strict Thunk | Shallow Thunk
-- | A Tree of Expressions.
type ExpressionTree = Tree Expression

-- | Fix represents any Fix type, including both Expressions and Relations.
data Fix = Expression Expression | Relation Relation
-- | A Relation represents either the Application of a Tree or the Evaluation of an Object.
data Relation = Think Thunk | Eval Object

-- * Functions

-- ** Runtime-Provided Functions

-- | Map a function over the elements of a Tree.  
treeMap :: (a -> b) -> Tree a -> Tree b
treeMap f x = case x of
    Tree x' -> Tree $ map (name . f . load) x'
    Tag x' -> Tag $ map (name . f . load) x'

-- | Given a name, produce its data.
load :: Name a -> a
load = undefined

-- | Given a name of a tree, produce a shallow copy of its data.
loadShallow :: Name (Tree a) -> [Name a]
loadShallow = undefined

-- | Given data, compute its name.
name :: a -> Name a
name = undefined

-- | Apply a function described by a combination.
apply :: ObjectTree -> Object
apply _ = undefined

-- | Select data as specified by an ObjectTree, without loading or evaluating the rest of the tree.
select :: ObjectTree -> Object
select _ = undefined

-- ** Evaluation Rules

-- | Evaluate an Object by repeatedly replacing Thunks with their Values, producing a concrete value (not a Ref).
evalStrict :: Object -> Value
evalStrict (Value x) = lift x
evalStrict (Thunk x) = evalStrict $ think x
evalStrict (ObjectTree x) = ValueTree $ treeMap evalStrict x
evalStrict (ObjectTreeRef x) = ValueTree $ treeMap evalStrict $ load x

-- | Evaluate an Object by repeatedly replacing Thunks with their Values, producing a Ref.  This provides partial evaluation, as subtrees will not yet be evaluated.
evalShallow :: Object -> Object
evalShallow (Value x) = Value $ lower x
evalShallow (Thunk x) = evalShallow $ think x
evalShallow (ObjectTree x) = ObjectTreeRef $ name x
evalShallow (ObjectTreeRef x) = ObjectTreeRef x

-- | Execute one step of the evaluation of a Thunk.  This might produce another Thunk, or a Tree containing Thunks.
think :: Thunk -> Object
think (Identification x) = Value $ load x
think (Application x) = apply $ treeMap reduce $ load x
think (Selection x) = select $ treeMap evalShallow $ Tree $ loadShallow x

-- | Converts an Expression into an Object by executing any Encodes contained within the Expression.
reduce :: Expression -> Object
reduce (Object x) = x
reduce (Encode (Strict x)) = Value $ evalStrict $ Thunk x
reduce (Encode (Shallow x)) = evalShallow $ Thunk x
reduce (ExpressionTree x) = ObjectTree $ treeMap reduce x

-- | Convert a Value into a "concrete" Value, adding its data to the reachable set.
lift :: Value -> Value
lift (Blob x) = Blob x
lift (BlobRef x) = Blob $ load x
lift (ValueTree x) = ValueTree $ treeMap lift x
lift (ValueTreeRef x) = ValueTree $ treeMap lift $ load x

-- | Convert a Value into a "ref" Value, removing its data from the reachable set.
lower :: Value -> Value
lower (Blob x) = BlobRef $ name x
lower (BlobRef x) = BlobRef x
lower (ValueTree x) = ValueTreeRef $ name x
lower (ValueTreeRef x) = ValueTreeRef x

-- ** Forcing Functions (names subject to change)

-- | Given a Relation, finds the "result", otherwise passes Expressions through unchanged.
relate :: Fix -> Object
relate (Relation (Think x)) = think x
relate (Relation (Eval x)) = Value $ evalStrict x
relate (Expression x) = reduce x

-- | Evaluates anything in Fix to a Value.  The result will be concrete.  This is most likely what a user wants to do.
eval :: Fix -> Value
eval = lift . evalStrict . relate
