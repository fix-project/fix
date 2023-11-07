import Data.Set (Set)
import qualified Data.Set as Set

type StoredBlob  = [Char]
type StoredTree = [Handle]
data Data = BlobData StoredBlob | TreeData StoredTree deriving (Eq, Ord)

data Id = Id Int deriving (Eq, Ord)
data BlobId = BlobId Id deriving (Eq, Ord)
data TreeId = TreeId Id deriving (Eq, Ord)
data Value = Blob BlobId | Tree TreeId | Tag TreeId deriving (Eq, Ord)
data Name = Value Value | Thunk TreeId deriving (Eq, Ord)

data Accessibility = Lazy | Shallow | Strict deriving (Eq, Ord)
data Handle = Handle (Name, Accessibility) deriving (Eq, Ord)

data NameData = JustName Name | NameAndSize (Name, Int) | NameAndData (Name, Data) deriving (Eq, Ord)

loadBlob :: BlobId -> StoredBlob
loadBlob = undefined

loadTree :: TreeId -> StoredTree
loadTree = undefined

deref :: Name -> Data
deref name = case name of
  Value (Blob id) -> BlobData $ loadBlob id
  Value (Tree id) -> TreeData $ loadTree id
  Value (Tag id) -> TreeData $ loadTree id
  Thunk id -> TreeData $ loadTree id

size :: Data -> Int
size (BlobData b) = length b
size (TreeData t) = length t

dat :: Handle -> NameData
dat handle = case handle of
  Handle (n, Lazy) -> JustName n
  Handle (Value v, Shallow) -> NameAndSize (Value v, size $ deref $ Value v)
  Handle (Value v, Strict) -> NameAndData (Value v, deref $ Value v)

apply :: StoredTree -> Handle
apply t = undefined

eval :: Name -> Value
eval o = undefined

minrepoHelper :: TreeId -> Set Handle
minrepoHelper id =
  foldr
    Set.union
    Set.empty
    (map minrepo $ loadTree id)

minrepo :: Handle -> Set Handle
minrepo handle = case handle of
  Handle (_, Lazy) -> Set.singleton handle
  Handle (Thunk t, _) -> Set.insert handle $ minrepoHelper t
  Handle (_, Shallow) -> Set.singleton $ handle
  Handle (Value (Blob b), Strict) -> Set.singleton $ handle
  Handle (Value (Tree t), Strict) -> Set.insert handle $ minrepoHelper t
  Handle (Value (Tag t), Strict) -> Set.insert handle $ minrepoHelper t

mindata :: Handle -> Set NameData
mindata = Set.map dat . minrepo

main :: IO ()
main = do
  return ()
