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

    # txn_core test - undo/redo transaction log over the IR and budget ceiling
    add_executable(test_txn_core unit/test_txn_core.cc)
    target_link_libraries(test_txn_core pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_txn_core PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_core)
    add_test(NAME test_txn_core COMMAND test_txn_core)

    # txn_replay test - replay + CLI byte-identical JSON
    add_executable(test_txn_replay unit/test_txn_replay.cc)
    target_link_libraries(test_txn_replay pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_txn_replay PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_replay)
    if (TYNYPDF_EXECUTABLES_CLI)
      add_dependencies(test_txn_replay ${TYNYPDF_EXECUTABLES_CLI})
    endif()
    add_test(NAME test_txn_replay COMMAND test_txn_replay)
    set_tests_properties(test_txn_replay PROPERTIES
      ENVIRONMENT "CLI_BINARY=$<TARGET_FILE:${TYNYPDF_EXECUTABLES_CLI}>"
    )

    # text_fallback test - font fallback per run: null reports capability not supported (R-M5),
    # mupdf splits the pt-BR fixture into face-pinned runs pinned against a byte golden, and a
    # missing glyph fails all-or-nothing with "missing glyph U+XXXX" (R20.x)
    add_executable(test_text_fallback unit/test_text_fallback.cc)
    target_link_libraries(test_text_fallback pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_text_fallback PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_text_fallback PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
        TEST_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
    pc_apply_warnings(test_text_fallback)
    add_test(NAME test_text_fallback COMMAND test_text_fallback)
    if (TARGET ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_link_libraries(test_text_fallback ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_compile_definitions(test_text_fallback PRIVATE PC_HAVE_MUPDF)
    endif()

    # text_break test - grapheme break positions with pt-BR hyphen and ABNT2 exceptions over the
    # golden pt-BR corpus, headless with no backend (R21.x)
    add_executable(test_text_break unit/test_text_break.cc)
    target_link_libraries(test_text_break pdfcore)
    target_include_directories(test_text_break PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_text_break PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
        TEST_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden")
    pc_apply_warnings(test_text_break)
    add_test(NAME test_text_break COMMAND test_text_break)

    # caret test - caret moves one grapheme cluster per step, never inside a combining sequence
    # or an ABNT2 composition, headless with no backend (R23.x)
    add_executable(test_caret unit/test_caret.cc)
    target_link_libraries(test_caret pdfcore)
    target_include_directories(test_caret PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_caret PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    pc_apply_warnings(test_caret)
    add_test(NAME test_caret COMMAND test_caret)

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

    # sidecar tests
    add_executable(test_sidecar_writer unit/test_sidecar_writer.cc)
    target_link_libraries(test_sidecar_writer pdfcore)
    target_include_directories(test_sidecar_writer PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_writer)
    add_test(NAME test_sidecar_writer COMMAND test_sidecar_writer)

    if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(test_sidecar_lock unit/test_sidecar_lock.cc)
    target_link_libraries(test_sidecar_lock pdfcore)
    target_include_directories(test_sidecar_lock PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_lock)
    add_test(NAME test_sidecar_lock COMMAND test_sidecar_lock)
    endif()

    add_executable(test_sidecar_unknown_keys unit/test_sidecar_unknown_keys.cc)
    target_link_libraries(test_sidecar_unknown_keys pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_sidecar_unknown_keys PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_unknown_keys)
    add_test(NAME test_sidecar_unknown_keys COMMAND test_sidecar_unknown_keys)

    add_executable(test_sidecar_schema unit/test_sidecar_schema.cc)
    target_link_libraries(test_sidecar_schema pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_sidecar_schema PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_schema)
    add_test(NAME test_sidecar_schema COMMAND test_sidecar_schema)

    add_executable(test_sidecar_ids unit/test_sidecar_ids.cc)
    target_link_libraries(test_sidecar_ids pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_sidecar_ids PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(test_sidecar_ids PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
    pc_apply_warnings(test_sidecar_ids)
    add_test(NAME test_sidecar_ids COMMAND test_sidecar_ids)

    if (NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(test_sidecar_staleness unit/test_sidecar_staleness.cc)
    target_link_libraries(test_sidecar_staleness pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_sidecar_staleness PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_staleness)
    add_test(NAME test_sidecar_staleness COMMAND test_sidecar_staleness)
    endif()

    add_executable(test_sidecar_reader unit/test_sidecar_reader.cc)
    target_link_libraries(test_sidecar_reader pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_sidecar_reader PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sidecar_reader)
    add_test(NAME test_sidecar_reader COMMAND test_sidecar_reader)

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

        # R-M2 threaded contract - per-doc lock sets must not serialize across threads.
        find_package(Threads REQUIRED)
        add_executable(test_backend_contract_threaded contract/threaded_contract.cc)
        target_link_libraries(test_backend_contract_threaded
            pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf Threads::Threads)
        target_include_directories(test_backend_contract_threaded PRIVATE ${CMAKE_SOURCE_DIR}/include)
        target_compile_definitions(test_backend_contract_threaded PRIVATE
            TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
        pc_apply_warnings(test_backend_contract_threaded)
        add_test(NAME test_backend_contract_threaded COMMAND test_backend_contract_threaded)
    endif()

    # window_swapchain test - the public window/swapchain ABI pins to portable types only
    # (R25.1, R15.2, R24.2); it compiles on Linux precisely because the headers leak no
    # Windows type (ADR-0011 R-M10)
    add_executable(test_window_swapchain unit/test_window_swapchain.cc)
    target_include_directories(test_window_swapchain PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_window_swapchain)
    add_test(NAME test_window_swapchain COMMAND test_window_swapchain)

    # uia test - the a11y document state and announced string (R24.7) are portable, so
    # test_uia compiles the state module directly and asserts the exact announced text the
    # docs/a11y scripts diff against. The COM provider (uia.cc) and the window plumbing are
    # win-only and are not linked here: this TU pins the portable contract, so flips in the
    # public surface break this build on Linux (ADR-0011 R-M10)
    add_executable(test_uia unit/test_uia.cc ${CMAKE_SOURCE_DIR}/src/os/win32/uia/uia_state.cc)
    target_include_directories(test_uia PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_uia)
    add_test(NAME test_uia COMMAND test_uia)

    # budget test - pc_budget struct and validation (R29.1, R29.2)
    add_executable(test_budget unit/test_budget.cc)
    target_link_libraries(test_budget pdfcore)
    target_include_directories(test_budget PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_budget)
    add_test(NAME test_budget COMMAND test_budget)

    # tiles test - tile cache LRU eviction (R27.1, R27.2)
    add_executable(test_tiles unit/test_tiles.cc)
    target_link_libraries(test_tiles pdfcore tynypdf-tiles)
    target_include_directories(test_tiles PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_tiles)
    add_test(NAME test_tiles COMMAND test_tiles)

    # tiles budget test - budget enforcement in tile cache (R27.3)
    add_executable(test_tiles_budget unit/test_tiles_budget.cc)
    target_link_libraries(test_tiles_budget pdfcore)
    target_include_directories(test_tiles_budget PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_tiles_budget)
    add_test(NAME test_tiles_budget COMMAND test_tiles_budget)

    # cachemap test - spatial tile query (R28.1, R28.2)
    add_executable(test_cachemap unit/test_cachemap.cc)
    target_link_libraries(test_cachemap pdfcore tynypdf-cachemap)
    target_include_directories(test_cachemap PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_cachemap)
    add_test(NAME test_cachemap COMMAND test_cachemap)

    # cachemap budget test - budget coordination (R28.3)
    add_executable(test_cachemap_budget unit/test_cachemap_budget.cc)
    target_link_libraries(test_cachemap_budget pdfcore)
    target_include_directories(test_cachemap_budget PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_cachemap_budget)
    add_test(NAME test_cachemap_budget COMMAND test_cachemap_budget)

    # txn budget test - transaction log budget enforcement (R29.2)
    add_executable(test_txn_budget unit/test_txn_budget.cc)
    target_link_libraries(test_txn_budget pdfcore)
    target_include_directories(test_txn_budget PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_txn_budget)
    add_test(NAME test_txn_budget COMMAND test_txn_budget)

    # viewer_loop test (story 1.5, R30.1) - the platform-neutral viewer loop
    # compiles and runs headless on Linux against the null backend (R-M10)
    add_executable(test_viewer_loop unit/test_viewer_loop.cc ${CMAKE_SOURCE_DIR}/src/app/viewer/viewer.cc)
    target_link_libraries(test_viewer_loop pdfcore tynypdf-tiles tynypdf-cachemap tynypdf-selection ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    target_include_directories(test_viewer_loop PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src/app)
    pc_apply_warnings(test_viewer_loop)
    add_test(NAME test_viewer_loop COMMAND test_viewer_loop)

    # corpus_contract test (story 1.5, R30.2) - the bench corpus is pinned by
    # sha256 and page count, opened through the mupdf backend; mupdf-only
    if (TARGET ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        add_executable(test_corpus_contract unit/test_corpus_contract.cc)
        target_link_libraries(test_corpus_contract pdfcore ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_include_directories(test_corpus_contract PRIVATE ${CMAKE_SOURCE_DIR}/include)
        target_compile_definitions(test_corpus_contract PRIVATE
            TEST_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/bench/corpus")
        pc_apply_warnings(test_corpus_contract)
        add_test(NAME test_corpus_contract COMMAND test_corpus_contract)
    endif()

    # sha256 test (story 1.5 fix) - FIPS 180-4 vectors and streaming
    # consistency; the corpus contract test found the original implementation
    # produced digests no other tool agreed with
    add_executable(test_sha256 unit/test_sha256.cc)
    target_link_libraries(test_sha256 pdfcore)
    target_include_directories(test_sha256 PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_sha256)
    add_test(NAME test_sha256 COMMAND test_sha256)

    # selection test (story 6.1, R32.1, R32.2) - hit-test against synthetic text layouts
    add_executable(test_selection unit/test_selection.cc)
    target_link_libraries(test_selection pdfcore tynypdf-selection)
    target_include_directories(test_selection PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_selection)
    add_test(NAME test_selection COMMAND test_selection)

    # cli_select test (story 6.1, R32.3) - CLI select subcommand
    add_executable(test_cli_select unit/test_cli_select.cc)
    target_link_libraries(test_cli_select pdfcore tynypdf-selection)
    target_include_directories(test_cli_select PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_cli_select)
    add_test(NAME test_cli_select COMMAND test_cli_select)

    # selection_sidecar test (story 6.1, R32.5) - sidecar round-trip
    add_executable(test_selection_sidecar unit/test_selection_sidecar.cc)
    target_link_libraries(test_selection_sidecar pdfcore)
    target_include_directories(test_selection_sidecar PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_warnings(test_selection_sidecar)
    add_test(NAME test_selection_sidecar COMMAND test_selection_sidecar)

endif()
