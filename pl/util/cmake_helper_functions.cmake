# contains functions to create a fixpoint-compatible wasm module out of a c++ object.


# link_libc links cc_obj_file with the libc and generates linked_with_libc.wasm. Depends on cc_obj_rule.
# Note: you also need to link with wasi_snapshot_preview1.wasm via wasmlink (see "link_wasi_stubs" function)
function(link_libc cc_obj_file cc_obj_rule) 
  add_custom_command(
    OUTPUT  "linked_with_libc.wasm"
    DEPENDS ${cc_obj_rule}
            ${cc_obj_file}
    COMMAND $ENV{HOME}/wasm-toolchain/sysroot/bin/wasm-ld
            ${cc_obj_file}
            $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc.a
            $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc++abi.a
            $ENV{HOME}/wasm-toolchain/sysroot/lib/wasm32-wasi/libc++.a
            $ENV{HOME}/wasm-toolchain/sysroot/lib/clang/16.0.0/lib/wasi/libclang_rt.builtins-wasm32.a
            -o linked_with_libc.wasm
            --no-entry
            --allow-undefined
  )
endfunction()

# link_storage links linked_with_libc.wasm with wasm module "storage_file_dir/storage_file_name".
# outputs linked_with_storage.wasm. The name of the rule that generates storage_file_name must be storage_file_name.
function(link_storage storage_file_dir storage_file_name)
    set_source_files_properties( ${storage_file_dir}/${storage_file_name} PROPERTIES GENERATED TRUE )

    # symlink because wasmlink requires modules to be in same directory
    add_custom_command(
        OUTPUT ${storage_file_name}
        DEPENDS ${storage_file_dir}/${storage_file_name}
        COMMAND ln -s ${storage_file_dir}/${storage_file_name} .)

    add_custom_command(
        OUTPUT "linked_with_storage.wasm"
        DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        linked_with_libc.wasm
        ${storage_file_name}
        COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        linked_with_libc.wasm
        ${storage_file_name}
        -o "linked_with_storage.wasm"
    )
endfunction()

# link_storage links linked_with_storage.wasm with wasm module "wasi_stubs_file_dir/wasi_stubs_file_name".
# outputs output_file_name. The name of the rule that generates wasi_stubs_file_name must be wasi_stubs_file_name.
function(link_wasi_stubs wasi_stubs_file_dir wasi_stubs_file_name output_file_name) 
    set_source_files_properties( ${wasi_stubs_file_dir}/${wasi_stubs_file_name} PROPERTIES GENERATED TRUE )

    # symlink because wasmlink requires modules to be in same directory
    add_custom_command(
        OUTPUT ${wasi_stubs_file_name}
        DEPENDS ${wasi_stubs_file_dir}/${wasi_stubs_file_name}
        COMMAND ln -s ${wasi_stubs_file_dir}/${wasi_stubs_file_name} .)

    add_custom_command(
        OUTPUT ${output_file_name}
        DEPENDS ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        linked_with_storage.wasm
        ${wasi_stubs_file_name}
        COMMAND ${CMAKE_BINARY_DIR}/_deps/wasm-tools-build/src/module-combiner/wasmlink
        --enable-multi-memory
        --enable-exceptions
        linked_with_storage.wasm
        ${wasi_stubs_file_name}
        -o ${output_file_name}
    )
endfunction()