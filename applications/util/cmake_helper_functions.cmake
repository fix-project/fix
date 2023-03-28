# link_libc links cc_obj_file with the libc and generates cc_wasm_name. Depends on cc_obj_rule.
# Note: you also need to link with wasi_snapshot_preview1.wasm via wasmlink.
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