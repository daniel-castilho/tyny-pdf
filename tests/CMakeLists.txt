if (TYNYPDF_BUILD_TESTS)
    # status_abi test
    add_executable(test_status_abi unit/test_status_abi.cc)
    target_link_libraries(test_status_abi pdfcore)
    target_include_directories(test_status_abi PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_status_abi)
    add_test(NAME test_status_abi COMMAND test_status_abi)

    # app_render test
    add_executable(test_app_render unit/test_app_render.cc)
    target_link_libraries(test_app_render pdfcore)
    target_include_directories(test_app_render PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_app_render)
    add_test(NAME test_app_render COMMAND test_app_render)

    # cli_exit_codes test
    add_executable(test_cli_exit_codes unit/test_cli_exit_codes.cc)
    target_link_libraries(test_cli_exit_codes pdfcore)
    target_include_directories(test_cli_exit_codes PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_cli_exit_codes)
    add_test(NAME test_cli_exit_codes COMMAND test_cli_exit_codes)

    # contract test
    add_executable(test_backend_contract contract/backend_contract.cc)
    target_link_libraries(test_backend_contract pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_backend_contract PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_backend_contract)
    add_test(NAME test_backend_contract COMMAND test_backend_contract)
endif()
