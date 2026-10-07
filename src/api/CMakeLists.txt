add_library(broime_api STATIC
    api.cpp
    native_ime.cpp
)

target_include_directories(broime_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
)

target_link_libraries(broime_api PUBLIC
    broime
    bronze_runtime_shared
)

target_compile_features(broime_api PUBLIC cxx_std_20)

broime_warnings(broime_api)
