add_custom_command(
  OUTPUT "addblob.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/addblob.wat
          ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
          --enable-multi-memory
          ${CMAKE_CURRENT_SOURCE_DIR}/addblob.wat
          -o addblob.wasm
)

add_custom_target(
  addblob_wasm ALL
  DEPENDS addblob.wasm
)
