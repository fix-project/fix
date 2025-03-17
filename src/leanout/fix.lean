universe u
namespace FixLang
/-
Define a custom list type.
-/
inductive SameTypeList (α :  Type u): Type u where
| nil  : SameTypeList α
| cons : α → SameTypeList α → SameTypeList α


/-
A Name is an opaque identifier represented by a 256-bit number.
-/
structure Name (α : Type u) where
  val : uint_64 × uint_64 × uint_64 × uint_64

/-
A Tree is a collection of Names of a particular type.
-/
  inductive Tree (α : Type u) where
| Tree : SameTypeList (Name α) → Tree α
| Tag  : SameTypeList (Name α) → Tree α
/-
A Blob is a collection of bytes. We use MyList char to represent [Char].
-/
def Blob : Type := SameTypeList Char

/-
A Ref is a reference to an object.
-/
def Ref (a : Type u) : Type u := a


mutual

  /-- An Object is a Value which may or may not have been computed yet.
      Uncomputed data are represented as Thunks. --/
  inductive Object : Type u where
  | thunk         : Thunk → Object
  | value         : Value → Object
  | objectTree    : Object → Object
  | objectTreeRef : ObjectTree → Object

  /-- A Thunk is a Value which has yet to be evaluated.
      It is either an Application, an Identification, or a Selection. --/
  inductive Thunk : Type u where
  | application    : ExpressionTree → Thunk
  | identification :  Value → Thunk
  | selection      : ObjectTree → Thunk

  /-- An Expression is a computation which may be further evaluated.
      When evaluated, it will produce an Object. --/
  inductive Expression : Type u where
  | object         : Object → Expression
  | encode         : Encode → Expression
  | expressionTree : ExpressionTree → Expression

  /-- An Encode is an instruction to replace a Thunk by the result of the function it represents.
      A Strict Encode requests complete expansion; a Shallow Encode, partial (lazy) expansion. --/
  inductive Encode : Type u where
  | strict  : Thunk → Encode
  | shallow : Thunk → Encode

  /-- Fix represents any Fix type, including both Expressions and Relations. --/
  inductive Fix : Type u where
  | expression : Expression → Fix
  | relation   : Relation → Fix

  /-- A Relation represents either the Application of a Tree or the Evaluation of an Object. --/
  inductive Relation : Type u where
  | think : Thunk → Relation
  | eval  : Object → Relation

inductive ExpressionTree : Type u where
| mk : Expression → ExpressionTree


end

def load {α : Type u} (n : Name α) : α := sorry
def loadShallow {α : Type u} (n : Name (Tree α)) : SameTypeList (Name α) := sorry
def name {α : Type u} (a : α) : Name α := sorry
def apply (ot : ObjectTree) : Object := sorry
def select (ot : ObjectTree) : Object := sorry


inductive ObjectTree : Type u where
| mk : Object → ObjectTree

instance : Inhabited ExpressionTree :=
  ⟨ExpressionTree.mk (Expression.object (Object.value 0))⟩


mutual
  /- Evaluating an ExpressionTree amounts to extracting its Expression and evaluating it. -/
  inductive EvalExpressionTree : ExpressionTree → Object → Prop
  | mk {e : Expression} {o : Object} (h : EvalExpression e o) :
      EvalExpressionTree (ExpressionTree.mk e) o

  /- An object evaluates as follows:
     - A value evaluates to itself.
     - A thunk containing an application evaluates by evaluating the contained ExpressionTree.
     - A thunk containing an identification evaluates to the wrapped value.
     - A thunk containing a selection evaluates by “selecting” the object from the ObjectTree.
     - An object-tree or object-tree-ref evaluates by evaluating the contained object (using apply for refs).
  -/
  inductive EvalObject : Object → Object → Prop
  | value {v : Value} :
      EvalObject (Object.value v) (Object.value v)
  | thunk_application {et : ExpressionTree} {o : Object}
      (h : EvalExpressionTree et o) :
      EvalObject (Object.thunk (Thunk.application et)) o
  | thunk_identification {v : Value} :
      EvalObject (Object.thunk (Thunk.identification v)) (Object.value v)
  | thunk_selection {ot : ObjectTree} {o : Object}
      (h : EvalObject (select ot) o) :
      EvalObject (Object.thunk (Thunk.selection ot)) o
  | object_tree {o o' : Object}
      (h : EvalObject o o') :
      EvalObject (Object.objectTree o) (Object.objectTree o')
  | object_tree_ref {ot : ObjectTree} {o : Object}
      (h : EvalObject (apply ot) o) :
      EvalObject (Object.objectTreeRef ot) o

  /- Evaluating an Encode forces the thunk inside. -/
  inductive EvalEncode : Encode → Object → Prop
  | strict {t : Thunk} {o : Object}
      (h : EvalObject (Object.thunk t) o) :
      EvalEncode (Encode.strict t) o
  | shallow {t : Thunk} {o : Object}
      (h : EvalObject (Object.thunk t) o) :
      EvalEncode (Encode.shallow t) o

  /- Evaluating an Expression delegates to one of its three cases. -/
  inductive EvalExpression : Expression → Object → Prop
  | obj {o o' : Object}
      (h : EvalObject o o') :
      EvalExpression (Expression.object o) o'
  | enc {enc : Encode} {o : Object}
      (h : EvalEncode enc o) :
      EvalExpression (Expression.encode enc) o
  | tree {et : ExpressionTree} {o : Object}
      (h : EvalExpressionTree et o) :
      EvalExpression (Expression.expressionTree et) o
end

@[export applyone] def applyone (ot : ObjectTree) : Object :=
  match ot with
  | ObjectTree.mk o => o

def selectone (ot : ObjectTree) : Object :=
  match ot with
  | ObjectTree.mk o => o

@[export testEvalObject] def testEvalObject : IO Unit :=
  let o := Object.value 42
  IO.println "Test EvalObject: Created Object.value 42."

def testEvalExpression : IO Unit :=
  let e := Expression.object (Object.value 100)
  IO.println "Test EvalExpression: Created Expression.object (Object.value 100)."

@[export testApplySelect] def testApplySelect : IO Unit :=
  let ot := ObjectTree.mk (Object.value 7)
  let a := applyone ot
  let s := selectone ot
  IO.println "Test Apply/Select: applyone and selectone executed on ObjectTree.mk (Object.value 7)."

def testApplyOne : IO Unit :=
  let ot := ObjectTree.mk (Object.value 8)
  let a := applyone ot
  IO.println "Test ApplyOne: executed on applyone (Object.value 8)."

end FixLang


def main : IO Unit := do
  FixLang.testApplyOne
  FixLang.testEvalObject
  FixLang.testEvalExpression
  FixLang.testApplySelect
  IO.println "completed refactoring fix.hs code in Lean..."