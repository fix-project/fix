#pragma once

typedef unsigned char __attribute__( ( vector_size( 32 ) ) ) u8x32;
typedef unsigned long long __attribute__( ( vector_size( 32 ) ) ) u64x4;

struct Named;
struct Literal;
struct Value;
struct Object;
struct Expression;

template<typename T>
struct Handle;

template<typename T>
struct Tree;

template<typename T>
struct TreeRef;

using ValueTree = Tree<Value>;
using ObjectTree = Tree<Object>;
using ExpressionTree = Tree<Expression>;
using ValueTreeRef = TreeRef<Value>;
using ObjectTreeRef = TreeRef<Object>;
