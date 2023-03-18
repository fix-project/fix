use heck::ShoutySnakeCase;
use std::collections::HashMap;
use witx::*;

pub struct Generated {
    pub header: String,
    pub source: String,
}

pub fn to_c(doc: &Document, inputs_str: &str) -> Generated {
    let mut header = String::new();

    header.push_str(&format!(
        r#"/**
 * THIS FILE IS AUTO-GENERATED from the following files:
 *   {}
 *
 * To regenerate this file execute:
 *
 *     cargo run --manifest-path tools/wasi-headers/Cargo.toml generate-flatware
 *
 * Modifications to this file will cause CI to fail, the code generator tool
 * must be modified to change this file.
 *
 * @file
 * This file describes the [WASI] interface, consisting of functions, types,
 * and defined values (macros).
 *
 * The interface described here is greatly inspired by [CloudABI]'s clean,
 * thoughtfully-designed, capability-oriented, POSIX-style API.
 *
 * [CloudABI]: https://github.com/NuxiNL/cloudlibc
 * [WASI]: https://github.com/WebAssembly/WASI/
 */

#include <stddef.h>
#include <stdint.h>

_Static_assert(_Alignof(int8_t) == 1, "non-wasi data layout");
_Static_assert(_Alignof(uint8_t) == 1, "non-wasi data layout");
_Static_assert(_Alignof(int16_t) == 2, "non-wasi data layout");
_Static_assert(_Alignof(uint16_t) == 2, "non-wasi data layout");
_Static_assert(_Alignof(int32_t) == 4, "non-wasi data layout");
_Static_assert(_Alignof(uint32_t) == 4, "non-wasi data layout");
_Static_assert(_Alignof(int64_t) == 8, "non-wasi data layout");
_Static_assert(_Alignof(uint64_t) == 8, "non-wasi data layout");
_Static_assert(_Alignof(void*) == 4, "non-wasi data layout");

#ifdef __cplusplus
extern "C" {{
#endif

// TODO: Encoding this in witx.
#define __WASI_DIRCOOKIE_START (UINT64_C(0))
"#,
        inputs_str,
    ));

    let mut source = String::new();
    source.push_str(&format!(
        r#"/**
 * THIS FILE IS AUTO-GENERATED from the following files:
 *   {}
 *
 * To regenerate this file execute:
 *
 *     cargo run --manifest-path tools/wasi-headers/Cargo.toml generate-flatware
 *
 * Modifications to this file will cause CI to fail, the code generator tool
 * must be modified to change this file.
 */

#include <stdint.h>

"#,
        inputs_str,
    ));

    let mut type_constants = HashMap::new();
    for c in doc.constants() {
        type_constants.entry(&c.ty).or_insert(Vec::new()).push(c);
    }

    for nt in doc.typenames() {
        print_datatype(&mut header, &*nt);

        if let Some(constants) = type_constants.remove(&nt.name) {
            for constant in constants {
                print_constant(&mut header, &constant);
            }
        }
    }

    for m in doc.modules() {
        print_module(&mut header, &mut source, &m);
    }

    header.push_str(
        r#"#ifdef __cplusplus
}
#endif

#endif
"#,
    );

    Generated { header, source }
}

fn print_datatype(ret: &mut String, nt: &NamedType) {
    if !nt.docs.is_empty() {
        ret.push_str("/**\n");
        for line in nt.docs.lines() {
            ret.push_str(&format!(" * {}\n", line));
        }
        ret.push_str(" */\n");
    }

    match &nt.tref {
        TypeRef::Value(v) => match &**v {
            Type::Record(s) => print_record(ret, &nt.name, s),
            Type::Variant(v) => print_variant(ret, &nt.name, v),
            Type::Handle(h) => print_handle(ret, &nt.name, h),
            Type::Builtin { .. }
            | Type::List { .. }
            | Type::Pointer { .. }
            | Type::ConstPointer { .. } => print_alias(ret, &nt.name, &nt.tref),
        },
        TypeRef::Name(_) => print_alias(ret, &nt.name, &nt.tref),
    }
}

fn print_alias(ret: &mut String, name: &Id, dest: &TypeRef) {
    match &**dest.type_() {
        Type::List(_) => {
            // Don't emit arrays as top-level types; instead we special-case
            // them in places like parameter lists so that we can pass them
            // as pointer and length pairs.
        }
        _ => {
            if name.as_str() == "size" {
                // Special-case "size" as "__SIZE_TYPE__" -- TODO: Encode this in witx.
                ret.push_str(&format!(
                    "typedef __SIZE_TYPE__ __wasi_{}_t;\n",
                    ident_name(name)
                ));
            } else {
                ret.push_str(&format!(
                    "typedef {} __wasi_{}_t;\n",
                    typeref_name(dest),
                    ident_name(name)
                ));
            }
            ret.push_str("\n");

            ret.push_str(&format!(
                "_Static_assert(sizeof(__wasi_{}_t) == {}, \"witx calculated size\");\n",
                ident_name(name),
                dest.mem_size_align().size
            ));
            ret.push_str(&format!(
                "_Static_assert(_Alignof(__wasi_{}_t) == {}, \"witx calculated align\");\n",
                ident_name(name),
                dest.mem_size_align().align
            ));

            ret.push_str("\n");
        }
    }
}

fn print_enum(ret: &mut String, name: &Id, v: &Variant) {
    ret.push_str(&format!(
        "typedef {} __wasi_{}_t;\n",
        intrepr_name(v.tag_repr),
        ident_name(name)
    ));
    ret.push_str("\n");

    for (index, case) in v.cases.iter().enumerate() {
        if !case.docs.is_empty() {
            ret.push_str("/**\n");
            for line in case.docs.lines() {
                ret.push_str(&format!(" * {}\n", line));
            }
            ret.push_str(" */\n");
        }
        ret.push_str(&format!(
            "#define __WASI_{}_{} ({}({}))\n",
            ident_name(&name).to_shouty_snake_case(),
            ident_name(&case.name).to_shouty_snake_case(),
            intrepr_const(v.tag_repr),
            index
        ));
        ret.push_str("\n");
    }

    ret.push_str(&format!(
        "_Static_assert(sizeof(__wasi_{}_t) == {}, \"witx calculated size\");\n",
        ident_name(name),
        v.tag_repr.mem_size()
    ));
    ret.push_str(&format!(
        "_Static_assert(_Alignof(__wasi_{}_t) == {}, \"witx calculated align\");\n",
        ident_name(name),
        v.tag_repr.mem_align()
    ));

    ret.push_str("\n");
}

fn print_constant(ret: &mut String, const_: &Constant) {
    if !const_.docs.is_empty() {
        ret.push_str("/**\n");
        for line in const_.docs.lines() {
            ret.push_str(&format!(" * {}\n", line));
        }
        ret.push_str(" */\n");
    }
    ret.push_str(&format!(
        "#define __WASI_{}_{} ((__wasi_{}_t){})\n",
        ident_name(&const_.ty).to_shouty_snake_case(),
        ident_name(&const_.name).to_shouty_snake_case(),
        ident_name(&const_.ty),
        const_.value,
    ));
    ret.push_str("\n");
}

fn print_record(ret: &mut String, name: &Id, s: &RecordDatatype) {
    if let Some(repr) = s.bitflags_repr() {
        ret.push_str(&format!(
            "typedef {} __wasi_{}_t;\n\n",
            intrepr_name(repr),
            ident_name(name)
        ));
        for (i, member) in s.members.iter().enumerate() {
            if !member.docs.is_empty() {
                ret.push_str("/**\n");
                for line in member.docs.lines() {
                    ret.push_str(&format!(" * {}\n", line));
                }
                ret.push_str(" */\n");
            }
            ret.push_str(&format!(
                "#define __WASI_{}_{} ((__wasi_{}_t)(1 << {}))\n",
                ident_name(name).to_shouty_snake_case(),
                ident_name(&member.name).to_shouty_snake_case(),
                ident_name(name),
                i,
            ));
            ret.push_str("\n");
        }
        return;
    }
    ret.push_str(&format!(
        "typedef struct __wasi_{}_t {{\n",
        ident_name(name)
    ));

    for member in &s.members {
        if !member.docs.is_empty() {
            ret.push_str("    /**\n");
            for line in member.docs.lines() {
                ret.push_str(&format!("     * {}\n", line));
            }
            ret.push_str("     */\n");
        }
        ret.push_str(&format!(
            "    {} {};\n",
            typeref_name(&member.tref),
            ident_name(&member.name)
        ));
        ret.push_str("\n");
    }

    ret.push_str(&format!("}} __wasi_{}_t;\n", ident_name(name)));
    ret.push_str("\n");

    ret.push_str(&format!(
        "_Static_assert(sizeof(__wasi_{}_t) == {}, \"witx calculated size\");\n",
        ident_name(name),
        s.mem_size()
    ));
    ret.push_str(&format!(
        "_Static_assert(_Alignof(__wasi_{}_t) == {}, \"witx calculated align\");\n",
        ident_name(name),
        s.mem_align()
    ));

    for layout in s.member_layout() {
        ret.push_str(&format!(
            "_Static_assert(offsetof(__wasi_{}_t, {}) == {}, \"witx calculated offset\");\n",
            ident_name(name),
            ident_name(&layout.member.name),
            layout.offset
        ));
    }

    ret.push_str("\n");
}

fn print_variant(ret: &mut String, name: &Id, v: &Variant) {
    if v.cases.iter().all(|v| v.tref.is_none()) {
        return print_enum(ret, name, v);
    }
    ret.push_str(&format!(
        "typedef union __wasi_{}_u_t {{\n",
        ident_name(name)
    ));

    for case in &v.cases {
        if let Some(tref) = &case.tref {
            if !case.docs.is_empty() {
                ret.push_str("    /**\n");
                for line in case.docs.lines() {
                    ret.push_str(&format!("    * {}\n", line));
                }
                ret.push_str("    */\n");
            }
            ret.push_str(&format!(
                "    {} {};\n",
                typeref_name(tref),
                ident_name(&case.name)
            ));
        }
    }
    ret.push_str(&format!("}} __wasi_{}_u_t;\n", ident_name(name)));

    ret.push_str(&format!(
        "typedef struct __wasi_{}_t {{\n",
        ident_name(name)
    ));

    ret.push_str(&format!("    {} tag;\n", intrepr_name(v.tag_repr)));
    ret.push_str(&format!("    __wasi_{}_u_t u;\n", ident_name(name)));

    ret.push_str(&format!("}} __wasi_{}_t;\n", ident_name(name)));
    ret.push_str("\n");

    ret.push_str(&format!(
        "_Static_assert(sizeof(__wasi_{}_t) == {}, \"witx calculated size\");\n",
        ident_name(name),
        v.mem_size()
    ));
    ret.push_str(&format!(
        "_Static_assert(_Alignof(__wasi_{}_t) == {}, \"witx calculated align\");\n",
        ident_name(name),
        v.mem_align()
    ));

    ret.push_str("\n");
}

fn print_handle(ret: &mut String, name: &Id, h: &HandleDatatype) {
    ret.push_str(&format!("typedef int __wasi_{}_t;\n\n", ident_name(name)));

    ret.push_str(&format!(
        "_Static_assert(sizeof(__wasi_{}_t) == {}, \"witx calculated size\");\n",
        ident_name(name),
        h.mem_size()
    ));
    ret.push_str(&format!(
        "_Static_assert(_Alignof(__wasi_{}_t) == {}, \"witx calculated align\");\n",
        ident_name(name),
        h.mem_align()
    ));

    ret.push_str("\n");
}

fn print_module(header: &mut String, source: &mut String, m: &Module) {
    header.push_str("/**\n");
    header.push_str(&format!(" * @defgroup {}\n", ident_name(&m.name),));
    for line in m.docs.lines() {
        header.push_str(&format!(" * {}\n", line));
    }
    header.push_str(" * @{\n");
    header.push_str(" */\n");
    header.push_str("\n");

    for func in m.funcs() {
        print_func_source(source, &func);
    }

    header.push_str("/** @} */\n");
    header.push_str("\n");
}

fn print_func_source(ret: &mut String, func: &InterfaceFunc) {
    let (params, results) = func.wasm_signature();

    let mut argnames = Vec::new();
    for param in func.params.iter() {
        argnames.push( ident_name(&param.name) );
        match &**param.tref.type_() {
            Type::List(ty) => match &**ty.type_() {
                _ => {
                    argnames.push(format!(
                        "{}_len", 
                        ident_name(&param.name)
                    ));
                }
            },
            _ => {}
        }
    }

    match func.results.len() {
        0 => {}
        1 => {
            assert!(!func.noreturn);
            push_return_argname(&mut argnames, &func.results[0].tref);
        }
        _ => panic!("unsupported number of return values"),
    }
    
    assert!(params.len() == argnames.len());
    if func.noreturn {
        ret.push_str("_Noreturn ");
    }
    match results.len() {
        0 => ret.push_str("void "),
        1 => {
            ret.push_str(wasm_type(&results[0]));
            ret.push_str(" ");
        }
        _ => unimplemented!(),
    }

    ret.push_str(&ident_name(&func.name));
    ret.push_str("(");
    for (i, param) in params.iter().enumerate() {
        if i > 0 {
            ret.push_str(", ");
        }
        ret.push_str(wasm_type(param));
        ret.push_str(" ");
        ret.push_str(&argnames[i]);
    }
    if params.len() == 0 {
      ret.push_str(" void ");
    }

    ret.push_str(") __attribute__((\n");
    ret.push_str(&format!(
        "    __export_name__(\"{}\")\n",
        ident_name(&func.name)
    ));
    ret.push_str("));\n\n");
}

fn push_return_argname(argnames: &mut Vec<String>, tref: &TypeRef) {
    match &**tref.type_() {
        Type::Variant(v) => {
            assert_eq!(v.cases.len(), 2, "unsupported type as a return value");
            assert!(
                v.cases[0].name == "ok",
                "unsupported type as a return value"
            );
            assert!(
                v.cases[1].name == "err",
                "unsupported type as a return value"
            );

            let ok = match &v.cases[0].tref {
                Some(ty) => ty,
                None => return,
            };
            match &**ok.type_() {
                Type::Record(r) if r.is_tuple() => {
                    for (i, _member) in r.members.iter().enumerate() {
                        argnames.push(format!("retptr{}", i));
                    }
                }
                _ => {
                    argnames.push(format!("retptr0"));
                }
            }
        }
        _ => panic!("unsupported type as a return value"),
    }
}

fn ident_name(i: &Id) -> String {
    i.as_str().to_string()
}

fn builtin_type_name(b: BuiltinType) -> &'static str {
    match b {
        BuiltinType::U8 { lang_c_char: true } => {
            panic!("no type name for string or char8 builtins")
        }
        BuiltinType::U8 { lang_c_char: false } => "uint8_t",
        BuiltinType::U16 => "uint16_t",
        BuiltinType::U32 {
            lang_ptr_size: true,
        } => "size_t",
        BuiltinType::U32 {
            lang_ptr_size: false,
        } => "uint32_t",
        BuiltinType::U64 => "uint64_t",
        BuiltinType::S8 => "int8_t",
        BuiltinType::S16 => "int16_t",
        BuiltinType::S32 => "int32_t",
        BuiltinType::S64 => "int64_t",
        BuiltinType::F32 => "float",
        BuiltinType::F64 => "double",
        BuiltinType::Char => "char32_t",
    }
}

fn typeref_name(tref: &TypeRef) -> String {
    match &**tref.type_() {
        Type::Builtin(BuiltinType::U8 { lang_c_char: true }) | Type::List(_) => {
            panic!("unsupported grammar: cannot construct name of string or array",)
        }
        _ => {}
    }

    match tref {
        TypeRef::Name(named_type) => namedtype_name(&named_type),
        TypeRef::Value(anon_type) => match &**anon_type {
            Type::List(_) => unreachable!("arrays excluded above"),
            Type::Builtin(b) => builtin_type_name(*b).to_string(),
            Type::Pointer(p) => format!("{} *", typeref_name(&*p)),
            Type::ConstPointer(p) => format!("const {} *", typeref_name(&*p)),
            Type::Record { .. } | Type::Variant { .. } | Type::Handle { .. } => unreachable!(
                "wasi should not have anonymous structs, unions, enums, flags, handles"
            ),
        },
    }
}

fn namedtype_name(named_type: &NamedType) -> String {
    match &**named_type.type_() {
        Type::Pointer(p) => format!("{} *", typeref_name(&*p)),
        Type::ConstPointer(p) => format!("const {} *", typeref_name(&*p)),
        Type::List(_) => unreachable!("arrays excluded above"),
        _ => format!("__wasi_{}_t", named_type.name.as_str()),
    }
}

fn intrepr_name(i: IntRepr) -> &'static str {
    match i {
        IntRepr::U8 => "uint8_t",
        IntRepr::U16 => "uint16_t",
        IntRepr::U32 => "uint32_t",
        IntRepr::U64 => "uint64_t",
    }
}

fn intrepr_const(i: IntRepr) -> &'static str {
    match i {
        IntRepr::U8 => "UINT8_C",
        IntRepr::U16 => "UINT16_C",
        IntRepr::U32 => "UINT32_C",
        IntRepr::U64 => "UINT64_C",
    }
}

fn wasm_type(wasm: &WasmType) -> &'static str {
    match wasm {
        WasmType::I32 => "int32_t",
        WasmType::I64 => "int64_t",
        WasmType::F32 => "float",
        WasmType::F64 => "double",
    }
}
