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

    # budget test
    add_executable(test_budget unit/test_budget.cc)
    target_link_libraries(test_budget pdfcore)
    target_include_directories(test_budget PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_budget)
    add_test(NAME test_budget COMMAND test_budget)

    # sidecar_writer test
    add_executable(test_sidecar_writer unit/test_sidecar_writer.cc)
    target_link_libraries(test_sidecar_writer pdfcore)
    target_include_directories(test_sidecar_writer PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_writer)
    add_test(NAME test_sidecar_writer COMMAND test_sidecar_writer)

    # sidecar_reader test
    add_executable(test_sidecar_reader unit/test_sidecar_reader.cc)
    target_link_libraries(test_sidecar_reader pdfcore)
    target_include_directories(test_sidecar_reader PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_reader)
    add_test(NAME test_sidecar_reader COMMAND test_sidecar_reader)

    # sidecar_ids test
    add_executable(test_sidecar_ids unit/test_sidecar_ids.cc)
    target_link_libraries(test_sidecar_ids pdfcore)
    target_include_directories(test_sidecar_ids PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_ids)
    add_test(NAME test_sidecar_ids COMMAND test_sidecar_ids)
    # sidecar_schema test
    add_executable(test_sidecar_schema unit/test_sidecar_schema.cc)
    target_link_libraries(test_sidecar_schema pdfcore)
    target_include_directories(test_sidecar_schema PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_schema)
    add_test(NAME test_sidecar_schema COMMAND test_sidecar_schema)
    # sidecar_staleness test
    add_executable(test_sidecar_staleness unit/test_sidecar_staleness.cc)
    target_link_libraries(test_sidecar_staleness pdfcore)
    target_include_directories(test_sidecar_staleness PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_staleness)
    add_test(NAME test_sidecar_staleness COMMAND test_sidecar_staleness)

    # txn_api test
    add_executable(test_txn_api unit/test_txn_api.cc)
    target_link_libraries(test_txn_api pdfcore)
    target_include_directories(test_txn_api PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_api)
    add_test(NAME test_txn_api COMMAND test_txn_api)

    # txn_log test
    add_executable(test_txn_log unit/test_txn_log.cc)
    target_link_libraries(test_txn_log pdfcore)
    target_include_directories(test_txn_log PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_log)
    add_test(NAME test_txn_log COMMAND test_txn_log)

    # txn_undo_redo test
    add_executable(test_txn_undo_redo unit/test_txn_undo_redo.cc)
    target_link_libraries(test_txn_undo_redo pdfcore)
    target_include_directories(test_txn_undo_redo PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_undo_redo)
    add_test(NAME test_txn_undo_redo COMMAND test_txn_undo_redo)

    # txn_replay test
    add_executable(test_txn_replay unit/test_txn_replay.cc)
    target_link_libraries(test_txn_replay pdfcore)
    target_include_directories(test_txn_replay PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_replay)
    add_test(NAME test_txn_replay COMMAND test_txn_replay)

    # txn_backend_independence test
    add_executable(test_txn_backend_independence unit/test_txn_backend_independence.cc)
    target_link_libraries(test_txn_backend_independence pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_txn_backend_independence PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_backend_independence)
    add_test(NAME test_txn_backend_independence COMMAND test_txn_backend_independence null)
    if (TARGET ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_link_libraries(test_txn_backend_independence ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_compile_definitions(test_txn_backend_independence PRIVATE PC_HAVE_MUPDF)
        add_test(NAME test_txn_backend_independence_mupdf COMMAND test_txn_backend_independence mupdf)
    endif()

    # txn_persistence test
    add_executable(test_txn_persistence unit/test_txn_persistence.cc)
    target_link_libraries(test_txn_persistence pdfcore)
    target_include_directories(test_txn_persistence PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_persistence)
    add_test(NAME test_txn_persistence COMMAND test_txn_persistence)

    # txn_log_clear test
    add_executable(test_txn_log_clear unit/test_txn_log_clear.cc)
    target_link_libraries(test_txn_log_clear pdfcore)
    target_include_directories(test_txn_log_clear PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_log_clear)
    add_test(NAME test_txn_log_clear COMMAND test_txn_log_clear)

    # txn_cli_replay test
    add_executable(test_txn_cli_replay unit/test_txn_cli_replay.cc)
    target_link_libraries(test_txn_cli_replay pdfcore)
    target_include_directories(test_txn_cli_replay PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_cli_replay)
    add_test(NAME test_txn_cli_replay COMMAND test_txn_cli_replay)

    # annotation_api test
    add_executable(test_annotation_api unit/test_annotation_api.cc)
    target_link_libraries(test_annotation_api pdfcore)
    target_include_directories(test_annotation_api PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_annotation_api)
    add_test(NAME test_annotation_api COMMAND test_annotation_api)

    # annotation_query test
    add_executable(test_annotation_query unit/test_annotation_query.cc)
    target_link_libraries(test_annotation_query pdfcore)
    target_include_directories(test_annotation_query PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_annotation_query)
    add_test(NAME test_annotation_query COMMAND test_annotation_query)

    # annotation_json test
    add_executable(test_annotation_json unit/test_annotation_json.cc)
    target_link_libraries(test_annotation_json pdfcore)
    target_include_directories(test_annotation_json PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_annotation_json)
    add_test(NAME test_annotation_json COMMAND test_annotation_json)

    # annotation_persistence test
    add_executable(test_annotation_persistence unit/test_annotation_persistence.cc)
    target_link_libraries(test_annotation_persistence pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_annotation_persistence PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_annotation_persistence)
    add_test(NAME test_annotation_persistence COMMAND test_annotation_persistence)

    # render_no_undo test
    add_executable(test_render_no_undo unit/test_render_no_undo.cc)
    target_link_libraries(test_render_no_undo pdfcore)
    target_include_directories(test_render_no_undo PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_render_no_undo)
    add_test(NAME test_render_no_undo COMMAND test_render_no_undo)

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

    # text_normalize test
    add_executable(test_text_normalize unit/test_text_normalize.cc)
    target_link_libraries(test_text_normalize pdfcore)
    target_include_directories(test_text_normalize PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_text_normalize)
    add_test(NAME test_text_normalize COMMAND test_text_normalize)

    # text_break test
    add_executable(test_text_break unit/test_text_break.cc)
    target_link_libraries(test_text_break pdfcore)
    target_include_directories(test_text_break PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_text_break)
    add_test(NAME test_text_break COMMAND test_text_break)

    # caret test
    add_executable(test_caret unit/test_caret.cc)
    target_link_libraries(test_caret pdfcore)
    target_include_directories(test_caret PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/core/text)
    pc_apply_warnings(test_caret)
    add_test(NAME test_caret COMMAND test_caret)

    # text_runs test
    add_executable(test_text_runs unit/test_text_runs.cc)
    target_link_libraries(test_text_runs pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_text_runs PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_text_runs PRIVATE TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    pc_apply_warnings(test_text_runs)
    add_test(NAME test_text_runs COMMAND test_text_runs)
    set_tests_properties(test_text_runs PROPERTIES
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
