function(broime_test name)
    cmake_parse_arguments(T "" "TIMEOUT" "LIBS" ${ARGN})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE broime::broime ${T_LIBS})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
    broime_warnings(${name})
    if(NOT T_TIMEOUT)
        set(T_TIMEOUT 60)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${T_TIMEOUT} SKIP_RETURN_CODE 77)
endfunction()

broime_test(test_types)
broime_test(test_compose)
broime_test(test_xcompose_parser)
broime_test(test_candidate_model)
broime_test(test_anchor_geometry)
broime_test(test_dictionary_trie)
broime_test(test_ime_engine)

if(BROIME_ENABLE_API)
    add_executable(broime_test_api
        test_api.cpp
    )
    target_include_directories(broime_test_api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/api
        ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(broime_test_api PRIVATE
        broime_api
        broime
        bronze_runtime_shared
    )
    broime_warnings(broime_test_api)
    if(WIN32)
        target_compile_definitions(broime_test_api PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _WIN32_WINNT=0x0A00)
        add_custom_command(TARGET broime_test_api POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broime_test_api>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_LINKER_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broime_test_api>
        )
    endif()

    if(NOT BRASS_HOST_BACKEND)
        message(STATUS "broime: broime_test_api not registered (brass has no code generator for this target)")
    else()
        add_test(NAME broime_test_api COMMAND broime_test_api)
        set_tests_properties(broime_test_api PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)

        add_test(NAME broime_test_api_gc_stress COMMAND broime_test_api)
        set_tests_properties(broime_test_api_gc_stress PROPERTIES
            TIMEOUT 600
            SKIP_RETURN_CODE 77
            ENVIRONMENT "BRONZE_GC_STRESS=1;BRONZE_GC_POISON=1"
        )
        if(WIN32)
            set_tests_properties(broime_test_api PROPERTIES
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broime_test_api>;$ENV{PATH}")
            set_property(TEST broime_test_api_gc_stress APPEND PROPERTY
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broime_test_api>;$ENV{PATH}")
        endif()
    endif()
endif()
