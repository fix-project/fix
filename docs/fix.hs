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

-- | A Stub is a reference to an object.
type Stub a = Name a

-- | An Object is an already-computed datum.  It is safe to compare Objects for equality.
data Object = ObjectTree ObjectTree | ObjectTreeStub (Stub ObjectTree) | Blob Blob | BlobStub (Stub Blob)
-- | A Tree of Objects.
type ObjectTree = Tree Object

-- | A Value is a datum which may or may not have been computed yet.  Uncomputed data are represented as Thunks.
data Value = Thunk Thunk | Object Object | ValueTree ValueTree | ValueTreeStub (Stub ValueTree)
-- | A Thunk is a fully-applied function which has not yet been evaluated.  In the special case that the function is the identity function applied to an object, the result is expressed inline.
data Thunk = Combination (Name ExpressionTree) | Identity Object
-- | A Tree of Values.
type ValueTree = Tree Value

-- | An Expression is a computation which may be further evaluated.  When evaluated, it will produce a Value.
data Expression = Value Value | Encode Encode | ExpressionTree ExpressionTree
-- | An Encode is an instruction requesting that a Thunk be replaced by the result of the function it represents.  A Strict Encode requests the complete expansion of the result, while a Shallow Encode requests partial (i.e., lazy) expansion.
data Encode = Strict Thunk | Shallow Thunk
-- | A Tree of Expressions.
type ExpressionTree = Tree Expression

-- | Fix represents any Fix type, including both Expressions and Relations.
data Fix = Expression Expression | Relation Relation | FixTree FixTree
-- | A Relation represents either the application of a Thunk or the evaluation of an Expression.
data Relation = Apply Thunk | Eval Expression
-- | A Tree of Expressions and Relations.
type FixTree = Tree Fix

-- * Functions

-- ** Runtime-Provided Functions

-- | Map a function over the elements of a Tree.  
treeMap :: (a -> b) -> Tree a -> Tree b
treeMap f x = case x of
    Tree x' -> Tree $ map (name . f . load) x'
    Tag x' -> Tag $ map (name . f . load) x'

-- | Given a name, produce an object.
load :: Name a -> a
load = undefined

-- | Given an object, compute its name.
name :: a -> Name a
name = undefined

-- | Apply a function described by a combination.  Even though a Fixpoint procedure can attempt to return a non-Value Expression, doing so is considered a type error.
applyCombination :: Value -> Value
applyCombination _ = case (undefined :: Expression) of
                      Value x -> x
                      _ -> error "Result must be a Value."

-- ** Evaluation Rules

-- | Apply a Thunk.  In most cases this will apply a combination; however, as an optimization, a thunk describing an application of the identity function may be represented inline.
apply :: Thunk -> Value
apply (Identity x) = Object x
apply (Combination x) = applyCombination $ eval $ ExpressionTree $ load x

-- | Converts an Expression into a Value by executing any Encodes contained within the Expression.
eval :: Expression -> Value
eval (Value x) = x
eval (Encode (Strict x)) = eval $ encodeStrict $ apply x
eval (Encode (Shallow x)) = eval $ encodeShallow $ apply x
eval (ExpressionTree x) = ValueTree $ treeMap eval x

-- | Turns an arbitrary Value into an Expression which, when evaluated, will produce the full contents of that Value (containing neither Stubs nor Thunks).
encodeStrict :: Value -> Expression
encodeStrict (Thunk x) = Encode $ Strict x
encodeStrict (ValueTree x) = ExpressionTree $ treeMap encodeStrict x
encodeStrict (ValueTreeStub x) = ExpressionTree $ treeMap encodeStrict $ load x
encodeStrict (Object x) = Value $ Object $ lift x

-- | Turns an arbitrary Value into an Expression which, when evaluated, will produce a Stub corresponding to the value.
encodeShallow :: Value -> Expression
encodeShallow (Thunk x) = Encode $ Shallow x
encodeShallow (ValueTree x) = Value $ ValueTreeStub $ name x
encodeShallow (ValueTreeStub x) = Value $ ValueTreeStub x
encodeShallow (Object x) = Value $ Object $ lower x

-- | Convert a "stub" object into a "full" object, adding its data to the reachable set.  This may require doing additional computation if a stub was produced by lazy evaluation.
lift :: Object -> Object
lift (Blob x) = Blob x
lift (BlobStub x) = Blob $ load x
lift (ObjectTree x) = ObjectTree x
lift (ObjectTreeStub x) = ObjectTree $ load x

-- | Convert a "full" object into a "stub" object, removing its data from the reachable set.
lower :: Object -> Object
lower (Blob x) = BlobStub $ name x
lower (BlobStub x) = BlobStub x
lower (ObjectTree x) = ObjectTreeStub $ name x
lower (ObjectTreeStub x) = ObjectTreeStub x

-- ** Forcing Functions

-- | Turns a Value into an Object by repeatedly replacing Thunks with their results.
force :: Value -> Object
force (Object x) = x
force (ValueTree x) = ObjectTree $ treeMap force x
force (ValueTreeStub x) = ObjectTree $ treeMap force $ load x
force (Thunk x) = force $ apply x

-- | Produces the Expression corresponding to any Fix type, including a Relation.
reduce :: Fix -> Expression
reduce (Relation (Apply x)) = Value $ apply x
reduce (Relation (Eval x)) = Value $ eval x
reduce (FixTree x) = ExpressionTree $ treeMap reduce x
reduce (Expression x) = x

-- | Produces the concrete Object corresponding to a Fix type.  This will perform all computation and gather all the data entailed by the relation.
perform :: Fix -> Object
perform = lift . force . eval . reduce
