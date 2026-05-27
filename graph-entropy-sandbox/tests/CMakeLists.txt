add_executable(entropy_exp_a_tests
    unit/test_entropy2d.cpp
    unit/test_sbm_gen.cpp
    unit/test_estimators.cpp
    unit/test_union_invariants.cpp
    unit/test_reproducibility.cpp
    unit/test_csv_schema.cpp
    golden/test_golden_entropy2d.cpp
)

target_link_libraries(entropy_exp_a_tests PRIVATE
    entropy_core
    Catch2::Catch2WithMain
)

# Force plugin objects into the test binary (needed for static initializers)
if(MSVC)
    target_link_options(entropy_exp_a_tests PRIVATE
        /WHOLEARCHIVE:$<TARGET_FILE:entropy_plugins>)
else()
    target_link_libraries(entropy_exp_a_tests PRIVATE
        -Wl,--whole-archive
        $<TARGET_OBJECTS:entropy_plugins>
        -Wl,--no-whole-archive
    )
endif()

apply_compiler_warnings(entropy_exp_a_tests)

include(Catch)
file(COPY ${CMAKE_SOURCE_DIR}/tests/golden/reference_values.json
     DESTINATION ${CMAKE_BINARY_DIR}/tests/golden/)
catch_discover_tests(entropy_exp_a_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
