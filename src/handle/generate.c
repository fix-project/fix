#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type
{
  const char* name;
  size_t bits;
  bool fix;
  size_t num_options;
  struct Type* options[];
} Type;

Type* make_terminal( const char* name, size_t bits )
{
  Type* value = malloc( sizeof( Type ) );
  *value = ( Type ) {
    .name = name,
    .fix = true,
    .bits = bits,
    .num_options = 0,
  };
  return value;
}

Type* make_sum( const char* name, size_t n, ... )
{
  Type* value = malloc( sizeof( Type ) + n * sizeof( Type* ) );
  size_t tag_bits = ceil( log2( n ) );
  size_t bits = 0;
  *value = ( Type ) {
    .name = name,
    .fix = true,
    .num_options = n,
  };
  va_list args;
  va_start( args, n );
  for ( unsigned i = 0; i < n; i++ ) {
    Type* next = va_arg( args, Type* );
    value->options[i] = next;
    if ( next->bits > bits ) {
      bits = next->bits;
    };
  }
  va_end( args );
  value->bits = tag_bits + bits;
  return value;
}

Type* make_wrapper( const char* name, const Type* type )
{
  return make_sum( name, 1, type );
}

void print_type( const Type* type )
{
  if ( type->num_options != 0 ) {
    printf( "%s := %s", type->name, type->options[0]->name );
    for ( size_t i = 1; i < type->num_options; i++ ) {
      printf( " %s", type->options[i]->name );
    }
    printf( " (%lu bits)\n", type->bits );
  } else {
    printf( "%s => %lu bits\n", type->name, type->bits );
  }
}

void serialize_mask( unsigned offset, unsigned tag_bits )
{
  printf( "\tstatic constexpr u8x32 mask = {" );
  for ( size_t i = 0; i < 256; i++ ) {
    if ( i % 8 == 0 ) {
      if ( i )
        printf( ", " );
      if ( i % 64 == 0 )
        printf( "\n\t\t" );
      printf( "0b" );
    }
    if ( i < ( 256 - offset - tag_bits ) ) {
      printf( "0" );
    } else if ( i < ( 256 - offset ) ) {
      printf( "1" );
    } else {
      printf( "0" );
    }
  }
  printf( "\n" );
  printf( "\t};\n" );
}

void serialize_enum( const Type* type, unsigned offset, unsigned tag_bits )
{
  printf( "\tenum class Option {\n" );
  for ( unsigned i = 0; i < type->num_options; i++ ) {
    const char* name = type->options[i]->name;
    printf( "\t\t%s,\n", name );
  }
  printf( "\t};\n\n" );
  printf( "\tinline Option option() const {\n" );
  printf( "\t\tconst u64x4 result = (u64x4)content & (u64x4)mask;\n" );
  unsigned long long mask = ( (unsigned long long)pow( 2, tag_bits ) - 1 ) << offset;
  unsigned be_offset = ( offset % 8 ) + ( 7 - ( offset / 8 ) ) * 8;
  printf(
    "\t\tconst unsigned char option = (result[3] & 0x%016lx) >> %d;\n", __builtin_bswap64( mask ), be_offset );
  printf( "\t\tassert(option <= %lu);\n", type->num_options );
  printf( "\t\treturn (Option)option;\n" );
  printf( "\t}\n\n" );
}

void serialize_constructors( const Type* type, const unsigned offset )
{
  printf( "\tinline Handle() : content() {}\n" );
  printf( "private:\n" );
  printf( "\tinline Handle(const u8x32 &content) : content(content) {}\n" );
  printf( "public:\n" );
  printf( "\tstatic inline Handle<%s> forge(const u8x32 &content) {return "
          "{content};}\n\n",
          type->name );

  if ( type->num_options == 1 ) {
    const Type* option = type->options[0];
    printf( "\tinline Handle(const Handle<%s> &base) : "
            "content(base.content) {}\n\n",
            option->name );
  } else {
    for ( unsigned i = 0; i < type->num_options; i++ ) {
      const Type* option = type->options[i];
      printf( "\tinline Handle(const Handle<%s> &base) : content(base.content){\n", option->name );
      printf( "\t\tu64x4 words = (u64x4)content;\n" );
      unsigned long long mask = i << offset;
      printf( "\t\twords[3] |= 0x%016lx;\n", __builtin_bswap64( mask ) );
      printf( "\t\tcontent = (u8x32)words;\n" );
      printf( "\t}\n\n" );
    }
  }

  for ( unsigned i = 0; i < type->num_options; i++ ) {
    const Type* option = type->options[i];
    // wrapper types cause ambiguous implicit conversions, so we don't allow it
    if ( option->num_options != 1 ) {
      printf( "\ttemplate<FixType T>\n" );
      printf( "\tinline Handle(const Handle<T> &base) requires std::convertible_to<Handle<T>, "
              "Handle<%s>>: "
              "Handle(Handle<%s>(base)) {}\n\n",
              option->name,
              option->name );
    }
  }
}

void serialize_unwrap( const Type* type )
{
  printf( "\ttemplate<FixType T>\n" );
  printf( "\tinline Handle<T> unwrap() const {\n" );
  for ( unsigned i = 0; i < type->num_options; i++ ) {
    const Type* option = type->options[i];
    printf( "%sif constexpr (std::is_same<T, %s>::value) {\n", i == 0 ? "\t\t" : " else ", option->name );
    if ( type->num_options == 1 ) {
      printf( "\t\t\treturn Handle<%s>::forge(content);\n", type->options[0]->name );
    } else {
      printf( "\t\t\tassert(option() == Option::%s);\n", option->name );
      printf( "\t\t\treturn Handle<%s>::forge(content & ~mask);\n", option->name );
    }
    printf( "\t\t}" );
  }
  printf( " else {\n\t\t\tassert(false);\n\t\t}\n" );
  printf( "\t\t__builtin_unreachable();\n" );
  printf( "\t}\n\n" );
}

void serialize_try_into( const Type* type )
{
  printf( "\ttemplate<FixType T>\n" );
  printf( "\tinline std::optional<Handle<T>> try_into() const {\n" );
  for ( unsigned char i = 0; i < type->num_options; i++ ) {
    const Type* option = type->options[i];
    printf( "%sif constexpr (std::same_as<T, %s>) {\n", i == 0 ? "\t\t" : " else ", option->name );
    if ( type->num_options == 1 ) {
      printf( "\t\t\treturn unwrap<T>();\n" );
    } else {
      printf( "\t\t\tif (option() != Option::%s) return {};\n", option->name );
      printf( "\t\t\treturn unwrap<T>();\n" );
    }
    printf( "\t\t}" );
  }
  printf( " else {\n\t\t\treturn {};\n\t\t}\n" );
  printf( "\t}\n\n" );

  if ( type->num_options == 1 ) {
    printf( "\ttemplate<class Visitor>\n" );
    printf( "\tinline auto visit(Visitor v) {\n" );
    const Type* option = type->options[0];
    printf( "\t\treturn v(unwrap<%s>());\n", option->name );
    printf( "\t}\n\n" );
  } else {
    printf( "\ttemplate<typename Result, class Visitor>\n" );
    printf( "\tinline Result visit(Visitor v) {\n" );
    for ( unsigned i = 0; i < type->num_options; i++ ) {
      const Type* option = type->options[i];
      printf( "\t\tif (option() == Option::%s)\n", option->name );
      printf( "\t\t\treturn v(unwrap<%s>());\n", option->name );
    }
    printf( "\t\t__builtin_unreachable();\n" );
    printf( "\t}\n\n" );
  }

  printf( "\ttemplate<FixType A, FixType... As>\n" );
  printf( "\tinline auto contains() const {\n" );
  printf( "\t\tauto x = try_into<A>();\n" );
  printf( "\t\tif (!x) return false;\n" );
  printf( "\t\tif constexpr(sizeof...(As) == 0) {\n" );
  printf( "\t\t\treturn true;\n" );
  printf( "\t\t} else {\n" );
  printf( "\t\t\treturn std::visit([&](const auto y) -> bool {\n" );
  printf( "\t\t\t\tif constexpr(decltype(y)::is_fix_sum_type) {\n" );
  printf( "\t\t\t\t\treturn y.template contains<A, As...>();\n" );
  printf( "\t\t\t\t} else return false;\n" );
  printf( "\t\t\t}, x->get());\n" );
  printf( "\t\t}\n\n" );
  printf( "\t}\n\n" );
}

void serialize_get( const Type* type )
{
  printf( "\tinline std::variant<" );
  for ( size_t i = 0; i < type->num_options; i++ ) {
    if ( i != 0 )
      printf( ", " );
    printf( "Handle<%s>", type->options[i]->name );
  }
  printf( "> get() const {\n" );
  if ( type->num_options == 1 ) {
    printf( "\t\treturn unwrap<%s>();\n", type->options[0]->name );
  } else {
    printf( "\t\tswitch (option()) {\n" );
    for ( unsigned i = 0; i < type->num_options; i++ ) {
      const Type* option = type->options[i];
      printf( "\t\tcase Option::%s:\n", option->name );
      printf( "\t\t\treturn unwrap<%s>();\n", option->name );
    }
    printf( "\t\t}\n" );
    printf( "\t\t__builtin_unreachable();\n" );
  }
  printf( "\t}\n" );
}

void serialize( const Type* type )
{
  static const Type* serialized[2048];
  for ( unsigned i = 0; i < sizeof( serialized ) / sizeof( Type* ); i++ ) {
    if ( serialized[i] == type )
      return;
    if ( serialized[i] == NULL ) {
      serialized[i] = type;
      goto body;
    }
  }
  fprintf( stderr, "serialized cache too small!\n" );
  exit( 1 );

body:
  if ( type->num_options == 0 ) {
    goto assert_fix;
  }

  for ( size_t i = 0; i < type->num_options; i++ ) {
    serialize( type->options[i] );
  }
  const unsigned offset = 256 - type->bits;
  const bool wrapper = type->num_options == 1;
  unsigned tag_bits = ceil( log2( type->num_options ) );
  assert( tag_bits <= 8 );
  assert( offset + tag_bits < 64 );

  printf( "struct %s;\n\n", type->name );
  printf( "template<>\n" );
  printf( "struct Handle<%s> {\n", type->name );
  printf( "\tconstexpr static bool is_fix_data_type = true;\n" );
  printf( "\tconstexpr static bool is_fix_sum_type = true;\n" );
  printf( "\tconstexpr static bool is_fix_wrapper = %s;\n", wrapper ? "true" : "false" );
  if ( !wrapper ) {
    serialize_mask( offset, tag_bits );
  }
  printf( "\tu8x32 content;\n" );
  printf( "\n" );
  if ( !wrapper ) {
    serialize_enum( type, offset, tag_bits );
  }
  serialize_constructors( type, offset );

  serialize_unwrap( type );
  serialize_try_into( type );

  serialize_get( type );

  printf( "\ttemplate<FixType A>\n" );
  printf( "\tinline Handle<A> into() const requires "
          "std::constructible_from<Handle<A>, Handle<%s>> {\n",
          type->name );
  printf( "\t\treturn Handle<A>(*this);\n" );
  printf( "\t}\n\n" );

  printf( "\ttemplate<FixType A>\n" );
  printf( "\tinline Handle<A> into(size_t size) const requires "
          "std::constructible_from<Handle<A>, Handle<%s>, size_t> {\n",
          type->name );
  printf( "\t\treturn Handle<A>(*this,size);\n" );
  printf( "\t}\n" );

  printf( "};\n\n" );

  printf( "static inline std::ostream& operator<<(std::ostream& os, const Handle<%s>& h) {\n", type->name );
  printf( "\tos << \"%s (\";\n", type->name );
  printf( "\tstd::visit([&](auto x) {os << x;}, h.get());\n" );
  printf( "\tos << \")\";\n" );
  printf( "\treturn os;\n" );
  printf( "}\n\n" );

  if ( !wrapper && type->fix ) {
    printf( "template<typename T>\n" );
    printf( "concept Fix%sType = ", type->name );
    for ( unsigned i = 0; i < type->num_options; i++ ) {
      const Type* option = type->options[i];
      if ( i )
        printf( " or " );
      printf( "std::same_as<T, %s>", option->name );
      if ( option->num_options > 1 ) {
        printf( " or Fix%sType<T>", option->name );
      }
    }
    printf( ";\n\n" );
  }

assert_fix:
  if ( type->fix )
    printf( "static_assert(FixType<%s>);\n\n", type->name );
}

int main( int argc, char** argv )
{
  (void)argc;
  (void)argv;

  const Type* vtree = make_terminal( "ValueTree", 240 + 1 + 1 );      // hash + tag + is_local
  const Type* otree = make_terminal( "ObjectTree", 240 + 1 + 1 );     // hash + tag + is_local
  const Type* etree = make_terminal( "ExpressionTree", 240 + 1 + 1 ); // hash + tag + is_local

  const Type* vref = make_terminal( "ValueTreeRef", 240 + 1 + 1 );
  const Type* oref = make_terminal( "ObjectTreeRef", 240 + 1 + 1 );

  const Type* literal = make_terminal( "Literal", 240 + 5 ); // hash + size
  const Type* named = make_terminal( "Named", 240 + 1 );     // hash + is_local
  const Type* blob = make_sum( "Blob", 2, literal, named );
  const Type* bref = make_wrapper( "BlobRef", blob );

  const Type* value = make_sum( "Value", 4, blob, bref, vtree, vref );

  const Type* application = make_wrapper( "Application", etree );
  const Type* identification = make_wrapper( "Identification", value );
  const Type* selection = make_wrapper( "Selection", otree );
  const Type* thunk = make_sum( "Thunk", 3, application, identification, selection );
  const Type* object = make_sum( "Object", 4, value, thunk, otree, oref );

  const Type* strict = make_wrapper( "Strict", thunk );
  const Type* shallow = make_wrapper( "Shallow", thunk );
  const Type* encode = make_sum( "Encode", 2, strict, shallow );
  const Type* expression = make_sum( "Expression", 3, object, encode, etree );

  const Type* apply = make_wrapper( "Apply", otree );
  const Type* eval = make_wrapper( "Eval", object );
  const Type* relation = make_sum( "Relation", 2, apply, eval );
  const Type* fix = make_sum( "Fix", 2, expression, relation );

  fprintf( stderr, "Fix Handles currently need %lu bits.\n", fix->bits );
  assert( fix->bits <= 256 );

  printf( "#pragma once\n"
          "\n"
          "#include <cassert>\n"
          "#include <concepts> // IWYU pragma: keep\n"
          "#include <optional>\n"
          "#include <type_traits>\n"
          "#include <variant>\n"
          "\n"
          "#include \"handle_pre.hh\"\n"
          "\n"
          "template <typename T>\n"
          "concept FixType = Handle<T>::is_fix_data_type;\n"
          "template <typename T>\n"
          "concept FixHandle = T::is_fix_data_type;\n"
          "\n" );

  serialize( fix );
  const Type* anytree = make_sum( "AnyTree", 3, vtree, otree, etree );
  const Type* anytreeref = make_sum( "AnyTreeRef", 2, vref, oref );
  const Type* anydatatype = make_sum( "AnyDataType", 6, literal, named, vtree, otree, etree, relation );
  serialize( anytree );
  serialize( anytreeref );
  serialize( anydatatype );
  printf( "#include \"handle_post.hh\" // IWYU pragma: keep\n\n" );
}
