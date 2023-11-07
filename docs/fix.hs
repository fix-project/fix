import Data.Set (Set)
import qualified Data.Set as Set

-- | Describes, essentially, a file on disk/in RAM.  A Blob stores a sequence
-- of bytes, while a Tree stores a sequence of Handles.  Note that this usage
-- of "Blob" and "Tree" is slightly different than the Values below.
data Data = BlobData StoredBlob | TreeData StoredTree deriving (Eq, Ord)
type StoredBlob  = [Char]
type StoredTree = [Handle]

-- | This ID is some opaque identifier that uniquely identifies a specific
-- piece of data.  For example, this could be a sha256 hash of the data.
data Id = Id Int deriving (Eq, Ord)
data BlobId = BlobId Id deriving (Eq, Ord)
data TreeId = TreeId Id deriving (Eq, Ord)

-- | We can go from a BlobId or a TreeId to the stored equivalents, a
-- StoredBlob or a StoredTree.
class DataId a b where
  load :: a -> b
  create :: b -> a
instance DataId BlobId StoredBlob where
  load = undefined
  create = undefined
instance DataId TreeId StoredTree where
  load = undefined
  create = undefined

-- | A Value is a piece of literal data; it may be a Blob (arbitrary bytes), a
-- Tree (a sequence of Handles), or a Tag (a claim that a certain procedure has
-- assigned a piece of data a specific "type").
data Value = Blob BlobId | Tree TreeId | Tag TreeId deriving (Eq, Ord)

-- | A Name is a way to identify either a Value or a Thunk; Thunks represent
-- computations.  When evaluated, a Thunk turns into a Value.
data Name = Value Value | Thunk TreeId deriving (Eq, Ord)

-- | Sometimes a computation doesn't need access to a full Value, but only an
-- opaque reference to it that it can pass along to another computation.  A
-- Handle represents a certain access level for a certain name.
data Handle = Handle (Name, Accessibility) deriving (Eq, Ord)
-- | Strict accessibility means a running computation can see the full contents
-- of a Value, essentially calling `load` as described above.  Shallow
-- accessibility means a running computation can see certain information about
-- a Handle, including getting lazy versions of the first layer of Trees.  Lazy
-- accessibility means a running computation cannot inspect a Value at all, but
-- only include it in its result.
data Accessibility = Lazy | Shallow | Strict deriving (Eq, Ord)

-- | When sending the "data" of an object, we can be selective about what we
-- send.  Lazy Handles only need the name itself, Shallow Handles only need the
-- name and the size/length of the object, and Strict Handles need the full
-- data of an object.
data NameData = JustName Name | NameAndSize (Name, Int) | NameAndData (Name, Data) deriving (Eq, Ord)

-- | Look up the Data corresponding to a Name.
deref :: Name -> Data
deref name = case name of
  Value (Blob id) -> BlobData $ load id
  Value (Tree id) -> TreeData $ load id
  Value (Tag id) -> TreeData $ load id
  Thunk id -> TreeData $ load id

-- | Get the length of some Data.
size :: Data -> Int
size (BlobData b) = length b
size (TreeData t) = length t

-- | Given a Handle, select the Data it represents.
dat :: Handle -> NameData
dat handle = case handle of
  Handle (n, Lazy) -> JustName n
  Handle (Value v, Shallow) -> NameAndSize (Value v, size $ deref $ Value v)
  Handle (Value v, Strict) -> NameAndData (Value v, deref $ Value v)

-- | A core Fix operation; apply runs a computation represented by a Tree in a
-- specified (ENCODE) format.
apply :: Handle -> Handle
apply (Handle (Value (Tree _), Strict)) = undefined

-- | A core Fix operation; eval applies any strict or shallow Thunks within an
-- Object.
eval :: Handle -> Handle
eval handle = case handle of
  Handle (_, Lazy) -> handle
  Handle (Value (Blob b), _) -> handle
  Handle (Value (Tree t), Shallow) -> handle
  Handle (Value (Tag t), Shallow) -> handle
  Handle (Thunk t, Shallow) -> eval $ shallow $ apply $ eval $ encode t
  Handle (Value (Tree t), Strict) -> Handle(Value (Tree $ recurse t), Strict)
  Handle (Value (Tag t), Strict) -> Handle(Value (Tag $ recurse t), Strict)
  Handle (Thunk t, Strict) -> eval $ apply $ eval $ encode t
  where recurse = create . map eval . load
        encode t = Handle(Value (Tree t), Strict)
        shallow x = case x of
          Handle (Value (Tree t), Strict) -> Handle (Value (Tree t), Shallow)
          Handle (Value (Tag t), Strict) -> Handle (Value (Tag t), Shallow)
          h -> h

-- | Calculates the set of Handles which are necessary to begin application of
-- a Thunk.
attachableSet :: Handle -> Set Handle
attachableSet handle = case handle of
  Handle (_, Lazy) -> Set.singleton handle
  Handle (Thunk t, _) -> Set.insert handle $ recurse t
  Handle (_, Shallow) -> Set.singleton $ handle
  Handle (Value (Blob _), Strict) -> Set.singleton $ handle
  Handle (Value (Tree t), Strict) -> Set.insert handle $ recurse t
  Handle (Value (Tag t), Strict) -> Set.insert handle $ recurse t
  where recurse id = foldr
                      Set.union
                      Set.empty
                      (map attachableSet $ load id)

attachableData :: Handle -> Set NameData
attachableData = Set.map dat . attachableSet

-- | Calculates the set of Names which are necessary to fully evaluate an
-- object.
reachableSet :: Name -> Set Name
reachableSet name = case name of
  Value (Blob _) -> Set.singleton name
  Value (Tree t) -> Set.insert name $ recurse t
  Value (Tag t) -> Set.insert name $ recurse t
  Thunk t -> Set.insert name $ recurse t
  where recurse id = foldr
                      Set.union
                      Set.empty
                      (map (reachableSet . getName) $ load id)
        getName (Handle (n, _)) = n

main :: IO ()
main = do
  return ()
