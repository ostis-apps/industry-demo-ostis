add_library(monitoring-module SHARED agents.cpp assessment.cpp classification.cpp graph.cpp)
target_link_libraries(monitoring-module PRIVATE sc-machine::sc-memory)
set_target_properties(monitoring-module PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/extensions")

add_executable(monitoring-cases EXCLUDE_FROM_ALL test/monitoring_cases.cpp)
target_link_libraries(monitoring-cases PRIVATE sc-machine::sc-memory)
set(monitoring_case_args
    "${PROJECT_SOURCE_DIR}/knowledge-base/subject_domain_of_monitoring"
    "$<TARGET_FILE:monitoring-module>")
add_custom_target(check-monitoring
    COMMAND monitoring-cases ${monitoring_case_args}
    DEPENDS monitoring-module monitoring-cases
    USES_TERMINAL)
if(SC_BUILD_TESTS)
  set_target_properties(monitoring-cases PROPERTIES EXCLUDE_FROM_ALL FALSE)
  add_test(NAME monitoring-cases COMMAND monitoring-cases ${monitoring_case_args})
endif()
