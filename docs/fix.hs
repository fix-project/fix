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

-- | An Object is an already-computed datum.
data Object = ObjectTree ObjectTree | ObjectTreeStub (Stub ExpressionTree) | Blob Blob | BlobStub (Stub Blob)
-- | A Tree of Objects.
type ObjectTree = Tree Object

-- | A Value is a datum which may or may not have been computed yet.  Uncomputed data are represented as Thunks.
data Value = Thunk Thunk | Object Object | ValueTree ValueTree | ValueTreeStub (Stub ExpressionTree)
-- | A Thunk is a fully-applied function which has not yet been evaluated.  In the special case that the function is the identity function applied to an object, the result is expressed inline.
data Thunk = Combination (Name ExpressionTree) | Identity Object
-- | A Tree of Values.
type ValueTree = Tree Value

-- | An Expression is a computation which may be further evaluated.  When evaluated, it will produce a Value.
data Expression = Value Value | Encode Encode | ExpressionTree ExpressionTree | ExpressionTreeStub (Stub ExpressionTree)
-- | An Encode is an instruction expressing that a Thunk be replaced by the result of the function it represents.  A Strict Encode requests the complete expansion of the result, while a Shallow Encode requests partial (i.e., lazy) expansion.
data Encode = Strict Thunk | Shallow Thunk
-- | A Tree of Expressions.
type ExpressionTree = Tree Expression

-- | Fix represents any Fix type.  This may be an Expression or a Relation, or a sequence of Expressions and Relations.
data Fix = Expression Expression | Relation Relation | FixTree FixTree
-- | A Relation represents either the application of a Thunk or the evaluation of an Expression.
data Relation = Apply Thunk | Eval Expression
-- | A Tree of Fix types.
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

-- | Apply a function described by a combination.  The function may in theory produce any Expression; however, any Encodes or Stubs within it will be overridden by the Encode triggering this application.
applyCombination :: Value -> Expression
applyCombination = undefined

-- ** Evaluation Rules

-- | Convert a "full" object into a "stub" object, removing its data from the reachable set.
lower :: Object -> Object
lower (Blob x) = BlobStub $ name x
lower (BlobStub x) = BlobStub x
lower (ObjectTree x) = ObjectTreeStub $ name $ treeMap (Value . Object) x
lower (ObjectTreeStub x) = ObjectTreeStub x

-- | Convert a "stub" object into a "full" object, adding its data to the reachable set.  This may require doing additional computation if a stub was produced by lazy evaluation.
lift :: Object -> Object
lift (Blob x) = Blob x
lift (BlobStub x) = Blob $ load x
lift (ObjectTree x) = ObjectTree x
lift (ObjectTreeStub x) = ObjectTree $ treeMap (force . eval) $ load x

-- | Apply a Thunk.  In most cases this will apply a combination; however, as an optimization, a thunk describing an application of the identity function may be represented inline.
apply :: Thunk -> Expression
apply (Identity x) = Value $ Object x
apply (Combination x) = applyCombination $ eval $ ExpressionTree $ load x

-- | Converts an Expression into a Value by replacing any Encodes with the result of applying the Thunk to which they refer.  Encodes may additionally specify whether the replacement should be strict or shallow.
eval :: Expression -> Value
eval (Value x) = x
eval (Encode (Strict x)) = eval $ makeStrict $ apply x
eval (Encode (Shallow x)) = eval $ makeShallow $ apply x
eval (ExpressionTree x) = ValueTree $ treeMap eval x
eval (ExpressionTreeStub x) = ValueTreeStub x

-- | Turns an arbitrary Expression into a Strict version of that Expression; the result, when evaluated, will contain no Stubs nor Thunks.  This overrides Encodes already present within the Expression.
makeStrict :: Expression -> Expression
makeStrict (Encode (Strict x)) = Encode $ Strict x
makeStrict (Encode (Shallow x)) = Encode $ Strict x
makeStrict (ExpressionTree x) = ExpressionTree $ treeMap makeStrict x
makeStrict (ExpressionTreeStub x) = ExpressionTree $ treeMap makeStrict $ load x
makeStrict (Value (Thunk x)) = Encode $ Strict x
makeStrict (Value (ValueTree x)) = ExpressionTree $ treeMap (makeStrict . Value) x
makeStrict (Value (ValueTreeStub x)) = ExpressionTree $ treeMap makeStrict $ load x
makeStrict (Value (Object x)) = Value $ Object $ lift x

-- | Turns an arbitrary Expression into a Shallow version of that Expression; the result, when evaluated, will always be a Stub.  If the expression is itself an Encode, this overrides it.
makeShallow :: Expression -> Expression
makeShallow (Encode (Strict x)) = Encode $ Shallow x
makeShallow (Encode (Shallow x)) = Encode $ Shallow x
makeShallow (ExpressionTree x) = ExpressionTreeStub $ name x
makeShallow (ExpressionTreeStub x) = ExpressionTreeStub x
makeShallow (Value (Thunk x)) = Encode $ Shallow x
makeShallow (Value (ValueTree x)) = Value $ ValueTreeStub $ name $ treeMap Value x
makeShallow (Value (ValueTreeStub x)) = Value $ ValueTreeStub x
makeShallow (Value (Object x)) = Value $ Object $ lower x

-- | Turns a Value into an Object by repeatedly repacing Thunks with their results.
force :: Value -> Object
force (Object x) = x
force (ValueTree x) = ObjectTree $ treeMap force x
force (ValueTreeStub x) = ObjectTree $ treeMap (force . eval) $ load x
force (Thunk x) = force $ eval $ apply x

-- | Produces the Expression corresponding to any Fix type, including a Relation.
reduce :: Fix -> Expression
reduce (Relation (Apply x)) = apply x
reduce (Relation (Eval x)) = Value $ eval x
reduce (FixTree x) = ExpressionTree $ treeMap reduce x
reduce (Expression x) = x

-- | Produces the concrete Object corresponding to a Fix type.  This will perform all computation and gather all the data entailed by the relation.
perform :: Fix -> Object
perform = lift . force . eval . reduce
