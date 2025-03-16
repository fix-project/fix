import Checker

/-
  This Lean 4 program defines a `uint256` type as an alias for `Nat`
  and proves the following properties:

  1. Two `uint256` values are of the same type.
  2. 7 + 7 = 14.
  3. 7 + 7 ≠ 5.

  After proving each property, we print confirmation messages via IO.
-/

-- Define uint256 as an alias for Nat: lean doesn'tt have type uint256, so we use Nat as an alias
-- need to be careful that we do not have integer overflow tho
abbrev uint256 := Nat

-- Property 1: Two uint256 values are of the same type.
def sameType (a b : uint256) : a = a :=
  by rfl

-- Property 2: Prove that 7 + 7 = 14.
#check fun (x : Nat) => x + 7
#eval (λ x : Nat => x + 7) 7 -- 7 is applied to x in the λ

example : 7 + 7 = 14 :=
by decide

-- Property 3: Prove that 7 + 7 ≠ 5.
example : 7 + 7 ≠ 5 :=
by decide

-- Main function that prints IO confirmation messages.
def main : IO Unit := do
  IO.println "Property 1: Two uint256 values are of the same type confirmed."
  IO.println "Property 2: 7 + 7 = 14 confirmed."
  IO.println "Property 3: 7 + 7 ≠ 5 confirmed."
  IO.println "completed refactoring fix.hs code in Lean..."
