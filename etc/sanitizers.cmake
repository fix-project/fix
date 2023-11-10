option(USE_ASAN "Use address sanitizer")
option(USE_UBSAN "Use undefined-behavior sanitizer")
option(USE_TSAN "Use thread sanitizer")

set(USING_SANITIZER FALSE)

function(use_sanitizer NAME)
  set(USING_SANITIZER TRUE PARENT_SCOPE)
  add_compile_options(-fsanitize=${NAME})
  add_link_options(-fsanitize=${NAME})
endfunction()

if (USE_ASAN)
  use_sanitizer("address")
endif ()

if (USE_UBSAN)
  use_sanitizer("undefined")
endif ()

if (USE_TSAN)
  use_sanitizer("thread")
  # tsan may yield false positives when atomic_thread_fence is used
  add_compile_options(-Wno-error=tsan)
endif ()

if (USING_SANITIZER)
  add_compile_options(-fno-sanitize-recover=all)
  add_link_options(-fno-sanitize-recover=all)
endif ()
