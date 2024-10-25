#pragma once

typedef unsigned char __attribute__( ( vector_size( 32 ) ) ) u8x32;
typedef unsigned long long __attribute__( ( vector_size( 32 ) ) ) u64x4;

struct Named;
struct Literal;
struct Value;

template<typename T>
struct Handle;

struct Tree;
struct Tag;
struct TreeRef;
struct TagRef;
