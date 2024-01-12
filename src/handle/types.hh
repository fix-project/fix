#pragma once

typedef unsigned char __attribute__( ( vector_size( 32 ) ) ) u8x32;
typedef unsigned long long __attribute__( ( vector_size( 32 ) ) ) u64x4;

struct Named;
struct Literal;
struct Object;
struct Value;
struct Expression;
struct Fix;

template<typename T>
struct Handle;

template<typename T>
struct Tree;

using ObjectTree = Tree<Object>;
using ValueTree = Tree<Value>;
using ExpressionTree = Tree<Expression>;
using FixTree = Tree<Fix>;
