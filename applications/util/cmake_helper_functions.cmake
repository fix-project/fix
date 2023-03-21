# contains functions to create a fixpoint-compatible wasm module out of a c++ object.

# link_libc links cc_obj_file with the libc and generates cc_wasm_name. Depends on cc_obj_rule.
# Note: you also need to link with wasi_snapshot_preview1.wasm via wasmlink (see "link_wasi_stubs" function)
function(link_libc cc_obj_file cc_obj_rule cc_wasm_name) 
  add_custom_command(
    OUTPUT ${cc_wasm_name}
    DEPENDS ${cc_obj_rule}
            ${cc_obj_file}
    COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/wasm-ld
            ${cc_obj_file}
            $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc.a
            $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc++abi.a
            $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc++.a
            $ENV{HOME}/wasm-toolchain/sysroot/lib/clang/16.0.0/lib/wasi/libclang_rt.builtins-wasm32.a
            -o ${cc_wasm_name}
            --no-entry
            --allow-undefined
  )
endfunction()

# link_wasm links cc_wasm_name with wasm module "wasm_file_dir/wasm_file_name".
# outputs linked_with_wasm.wasm. The name of the rule that generates wasm_file_name 
# must be wasm_file_name and cc_wasm_name must be in the current directory.
function(link_wasm wasm_file_dir wasm_file_name cc_wasm_name)
    set_source_files_properties( ${wasm_file_dir}/${wasm_file_name} PROPERTIES GENERATED TRUE )

    if (NOT (${wasm_file_dir} STREQUAL ".")) 
        # copy because wasmlink requires modules to be in same directory
        add_custom_command(
            OUTPUT ${wasm_file_name}
            DEPENDS ${wasm_file_dir}/${wasm_file_name}
            COMMAND cp ${wasm_file_dir}/${wasm_file_name} .)
    endif()

    add_custom_command(
        OUTPUT "linked_with_wasm_0.wasm"
        DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        ${cc_wasm_name}
        ${wasm_file_name}
        COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        ${cc_wasm_name}
        ${wasm_file_name}
        -o "linked_with_wasm_0.wasm"
    )
endfunction()

# link_another_wasm links linked_with_wasm_{i-1}.wasm with wasm module "wasm_file_name".
# outputs linked_with_wasm_i.wasm. The name of the rule that generates wasm_file_name must be wasm_file_name.
function(link_another_wasm wasm_file_dir wasm_file_name wasm_index)
    set_source_files_properties( ${wasm_file_dir}/${wasm_file_name} PROPERTIES GENERATED TRUE )
    set(previous_wasm_index ${wasm_index})
    MATH(EXPR previous_wasm_index "${previous_wasm_index}-1")
    
    if (NOT (${wasm_file_dir} STREQUAL ".")) 
        # copy because wasmlink requires modules to be in same directory
        add_custom_command(
            OUTPUT ${wasm_file_name}
            DEPENDS ${wasm_file_dir}/${wasm_file_name}
            COMMAND cp ${wasm_file_dir}/${wasm_file_name} .)
    endif()

    add_custom_command(
        OUTPUT "linked_with_wasm_${wasm_index}.wasm"
        DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        linked_with_wasm_${previous_wasm_index}.wasm
        ${wasm_file_name}
        COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        "linked_with_wasm_${previous_wasm_index}.wasm"
        ${wasm_file_name}
        -o "linked_with_wasm_${wasm_index}.wasm"
    )
endfunction()

# link_wasm links linked_with_wasm_{num_wasms_linked - 1}.wasm with wasm module
# "wasi_stubs_file_dir/wasi_stubs_file_name".
# outputs output_file_name. The name of the rule that generates wasi_stubs_file_name must be wasi_stubs_file_name.
function(link_wasi_stubs wasi_stubs_file_dir wasi_stubs_file_name output_file_name num_wasms_linked) 
    set_source_files_properties( ${wasi_stubs_file_dir}/${wasi_stubs_file_name} PROPERTIES GENERATED TRUE )

    # copy because wasmlink requires modules to be in same directory
    add_custom_command(
        OUTPUT ${wasi_stubs_file_name}
        DEPENDS ${wasi_stubs_file_dir}/${wasi_stubs_file_name}
        COMMAND cp ${wasi_stubs_file_dir}/${wasi_stubs_file_name} .)

    set(wasm_index ${num_wasms_linked})
    MATH(EXPR wasm_index "${wasm_index}-1")

    add_custom_command(
        OUTPUT ${output_file_name}
        DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        linked_with_wasm_${wasm_index}.wasm
        ${wasi_stubs_file_name}
        COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        linked_with_wasm_${wasm_index}.wasm
        ${wasi_stubs_file_name}
        -o ${output_file_name}
    )
endfunction()