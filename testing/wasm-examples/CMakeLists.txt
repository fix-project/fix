add_custom_command(
  OUTPUT "addblob.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/addblob.wat
          ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
          --enable-multi-memory
          ${CMAKE_CURRENT_SOURCE_DIR}/addblob.wat
          -o addblob.wasm)

add_custom_target(addblob_wasm ALL DEPENDS addblob.wasm)

add_custom_command(
  OUTPUT "add-simple.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/add-simple.wat
          ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
          --enable-multi-memory
          ${CMAKE_CURRENT_SOURCE_DIR}/add-simple.wat
          -o add-simple.wasm)

add_custom_target(add_simple_wasm ALL DEPENDS add-simple.wasm)

add_custom_command(
  OUTPUT "exit.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exit.wat
          ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
          --enable-multi-memory
          ${CMAKE_CURRENT_SOURCE_DIR}/exit.wat
          -o exit.wasm)

add_custom_target(exit_wasm ALL DEPENDS exit.wasm)

add_custom_command(
  OUTPUT "fib.wasm"
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fib.wat
          ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
  COMMAND ${CMAKE_BINARY_DIR}/_deps/wabt-build/wat2wasm
          --enable-multi-memory
          ${CMAKE_CURRENT_SOURCE_DIR}/fib.wat
          -o fib.wasm)

add_custom_target(fib_wasm ALL DEPENDS fib.wasm)
