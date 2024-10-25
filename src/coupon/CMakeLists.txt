file (GLOB LIB_SOURCES couponcollector.cc scheduler.cc object.cc runtime.cc runtimestorage.cc hash_table.cc tagger.cc)

add_subdirectory(handle)

add_library(coupon STATIC ${LIB_SOURCES})
target_link_libraries(coupon PUBLIC handle-coupon util)

add_executable(coupon-test main.cc)
target_link_libraries(coupon-test coupon glog)
