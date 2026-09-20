if (TYNYPDF_BUILD_TESTS)
    # status_abi test
    add_executable(test_status_abi unit/test_status_abi.cc)
    target_link_libraries(test_status_abi pdfcore)
    target_include_directories(test_status_abi PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_status_abi PRIVATE TEST_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
    pc_apply_warnings(test_status_abi)
    add_test(NAME test_status_abi COMMAND test_status_abi)

    # geom test
    add_executable(test_geom unit/test_geom.cc)
    target_link_libraries(test_geom pdfcore)
    target_include_directories(test_geom PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_geom)
    add_test(NAME test_geom COMMAND test_geom)

    # doc_ir test
    add_executable(test_doc_ir unit/test_doc_ir.cc)
    target_link_libraries(test_doc_ir pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_doc_ir PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_doc_ir PRIVATE TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    pc_apply_warnings(test_doc_ir)
    add_test(NAME test_doc_ir COMMAND test_doc_ir)
    set_tests_properties(test_doc_ir PROPERTIES
      ENVIRONMENT "TEST_FIXTURE_DIR=${CMAKE_SOURCE_DIR}/tests/fixtures"
    )

    # app_render test - open document and render page 1 via pdfcore
    add_executable(test_app_render unit/test_app_render.cc)
    target_link_libraries(test_app_render pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_app_render PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_app_render)
    add_test(NAME test_app_render COMMAND test_app_render)
    set_tests_properties(test_app_render PROPERTIES
      ENVIRONMENT "CLI_BINARY=${CMAKE_BINARY_DIR}/Debug/tynypdf-cli;TEST_FIXTURE_DIR=${CMAKE_SOURCE_DIR}/tests/fixtures"
    )

    # cli_exit_codes test - invoke the CLI binary and assert exit codes + PNG output
    add_executable(test_cli_exit_codes unit/test_cli_exit_codes.cc)
    target_link_libraries(test_cli_exit_codes)
    target_include_directories(test_cli_exit_codes PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_cli_exit_codes)
    add_test(NAME test_cli_exit_codes COMMAND test_cli_exit_codes)
    set_tests_properties(test_cli_exit_codes PROPERTIES
      ENVIRONMENT "CLI_BINARY=${CMAKE_BINARY_DIR}/Debug/tynypdf-cli;TEST_FIXTURE_DIR=${CMAKE_SOURCE_DIR}/tests/fixtures"
    )

    # contract test - one binary, the same assertions against every installed backend
    add_executable(test_backend_contract contract/backend_contract.cc)
    target_link_libraries(test_backend_contract pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_backend_contract PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_backend_contract PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    pc_apply_warnings(test_backend_contract)
    add_test(NAME test_backend_contract COMMAND test_backend_contract null)
    if (TARGET ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_link_libraries(test_backend_contract ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_compile_definitions(test_backend_contract PRIVATE PC_HAVE_MUPDF)
        add_test(NAME test_backend_contract_mupdf COMMAND test_backend_contract mupdf)
    endif()
endif()
