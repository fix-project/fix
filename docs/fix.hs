{-# OPTIONS_GHC -fwarn-incomplete-patterns -Werror #-}
{-# LANGUAGE LambdaCase #-}

-- | The Fix language (types and evaluation rules)
module Fix where

import Data.Word (Word64)

-- * Fix Types

-- | A Name is some opaque identifier, in this case a 256-bit number.
-- | In a real implementation, Name Blob also includes the size of the Blob, and Name Tree includes the size, footprint, tag, and "eq" status.
newtype Name a = Name (Word64, Word64, Word64, Word64)

-- | A Blob is a collection of bytes.
type Blob = [Char]

-- | A Tree is a collection of handles of a particular type (at runtime, always Expression).
type Tree a = [a]

-- | A Ref is a reference to an inaccessible object (Blob, or Tree of Expressions)
data Ref = BlobRef (Name Blob) | TreeRef (Name (Tree Expression))

-- | An Object is a reference to an accessible object (Blob or Tree), parametrized by the type of Tree element.
data Object a = BlobObj (Name Blob) | TreeObj (Name (Tree a))

-- | Objects and Refs are "Data"
data Data a = Ref Ref | Object (Object a)

-- | A Thunk is a Value which has yet to be evaluated.  It is either described as an Application (of a function to arguments), an Identification (of an already-computed Value), or a Selection (of a particular element or subrange of a large structure).  It is better to use Identification or Selection Thunks where possible than applying an equivalent function, as these special Thunks have a smaller data footprint.
data Thunk = Application (Name (Tree Expression)) | Identification (Data Expression) | Selection (Name (Tree Expression))

-- | An Expression is a computation which may be further evaluated.  When evaluated, it will produce a Value.
data Expression = ExprData (Data Expression) | ExprThunk Thunk | ExprEncode Encode

-- | An Encode is an instruction requesting that a Thunk be replaced by the result of the function it represents, with a particular accessibility.
data Encode = Encode { thunk :: Thunk, accessible :: Bool }

-- | A Value is a Thunk, or Data where every accessible object is also a Value.
-- | This is only used for type-checking this model.
data Value = ValueData (Data Value) | ValueThunk Thunk

-- | Handle is a typeclass of things that can be converted to Expression (the more general type). At runtime, only Expression will exist.
class Handle a where
  relax :: a -> Expression

instance Handle Expression where
  relax = id

instance Handle Value where
  relax (ValueThunk t) = ExprThunk t
  relax (ValueData (Ref r)) = ExprData (Ref r)
  relax (ValueData (Object o)) = ExprData $ Object $ lift $ Object o

-- * Functions

-- ** Runtime-Provided Functions

-- | Map a function over the elements of a Tree.  
treeMap :: (Expression -> Maybe Value) -> Name (Tree Expression) -> Maybe(Name (Tree Value))
treeMap f x = fmap name $ traverse f $ load x

-- | Given a name, produce its data.
load :: Name a -> a
load = undefined

-- | Given data, compute its name.
name :: a -> Name a
name = undefined

-- | Apply a function as described by an evaluated combination: a tree that includes resource limits, the function, and the arguments/environment. This may trap, producing Nothing.
apply :: Name (Tree Value) -> Maybe Expression
apply _ = undefined

-- | Select data as specified, without loading or evaluating anything not needed. May trap, producing Nothing.
-- | The specification language is TBD, but will permit:
-- |  - fetching a byte range of a Blob
-- |  - fetching a single element of a Tree
-- |  - fetching a subrange of a Tree
-- |  - transforming each output element into a "digest" (a short Blob describing the available info from each Handle, to permit discovery of element types (Ref/Object Blob/Tree) and Name metadata)
select :: Name (Tree Expression) -> Maybe Expression
select _ = undefined

-- | Convert a Data into an Object, guaranteeing it is accessible.
lift :: Handle a => Data a -> Object Expression
lift (Object (BlobObj b)) = BlobObj b
lift (Object (TreeObj t)) = TreeObj $ name $ map relax $ load t
lift (Ref (BlobRef b)) = BlobObj $ name $ load b
lift (Ref (TreeRef t)) = TreeObj $ name $ map relax $ load t

-- | Convert a Data into a Ref, guaranteeing it is inaccessible.
lower :: Handle a => Data a -> Ref
lower (Ref x) = x
lower (Object (BlobObj x)) = BlobRef x
lower (Object (TreeObj x)) = TreeRef $ name $ map relax $ load x

-- ** Evaluation Rules

-- | Execute one step of the evaluation of a Thunk.  This might produce another Thunk, or a Tree containing Thunks.
think :: Thunk -> Maybe Expression
think (Identification x) = Just $ ExprData x
think (Selection x) = select x
think (Application x) = treeMap eval x >>= apply

-- | Think a Thunk until no more thoughts arrive (i.e. until it's Data).
force :: Thunk -> Maybe (Data Expression)
force thunk = think thunk >>= \case
  ExprThunk thunk' -> force thunk'
  ExprEncode (Encode thunk' _) -> force thunk' -- N.B. ignores accessible field of inner Encode. This is intended to lead to an equivalence between a Data and an Encode (of the correct accessibility) that, when executed, produces equivalent Data.
  ExprData d -> Just d

-- | Execute an Encode, producing Data of the desired accessibility.
-- | The Thunk is forced to Data, then the Data's accessibility is set.
execute :: Encode -> Maybe (Data Expression)
execute (Encode thunk accessible) = force thunk >>= Just . if accessible then Object . lift else Ref . lower

-- | Evaluates an Expression into a Value by executing any accessible Encodes contained within the Expression.
eval :: Expression -> Maybe Value
eval (ExprEncode e) = execute e >>= eval . ExprData
eval (ExprThunk t) = Just $ ValueThunk t
eval (ExprData (Ref r)) = Just $ ValueData $ Ref r
eval (ExprData (Object (BlobObj b))) = Just $ ValueData $ Object (BlobObj b)
eval (ExprData (Object (TreeObj t))) = ValueData . Object . TreeObj <$> treeMap eval t
