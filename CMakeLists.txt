cmake_minimum_required(VERSION 3.16)
project(tts_text_frontend VERSION 0.13.0 LANGUAGES C)
include(GNUInstallDirs)
include(CTest)
option(TTF_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(TTF_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(TTF_DEFAULT_RESOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/resources" CACHE PATH "Default resource directory (override through the API or CLI)")

file(GLOB_RECURSE TTF_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB TTF_PACK_DIRS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/languages/*")
set(TTF_PACK_SOURCES)
set(TTF_REGISTRY_DECLARATIONS "")
set(TTF_REGISTRY_CALLS "")
foreach(PACK_DIR IN LISTS TTF_PACK_DIRS)
  if(EXISTS "${PACK_DIR}/pack.c")
    get_filename_component(PACK_TAG "${PACK_DIR}" NAME)
    string(REPLACE "-" "_" PACK_SYMBOL "${PACK_TAG}")
    file(GLOB_RECURSE PACK_SOURCES CONFIGURE_DEPENDS "${PACK_DIR}/*.c")
    list(APPEND TTF_PACK_SOURCES ${PACK_SOURCES})
    string(APPEND TTF_REGISTRY_DECLARATIONS "extern const ttf_language_pack_t ttf_pack_${PACK_SYMBOL};\n")
    string(APPEND TTF_REGISTRY_CALLS "    status = ttf_engine_register_pack(e, &ttf_pack_${PACK_SYMBOL});\n    if (status != TTF_OK) return status;\n")
  endif()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/builtin_registry.c"
  "#include \"internal.h\"\n${TTF_REGISTRY_DECLARATIONS}\nttf_status_t ttf_register_builtins(ttf_engine_t *e) {\n    ttf_status_t status = TTF_OK;\n${TTF_REGISTRY_CALLS}    return status;\n}\n")
set(TTF_ALL_SOURCES ${TTF_CORE_SOURCES} ${TTF_PACK_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/builtin_registry.c")

add_library(ttf_build_flags INTERFACE)
if(MSVC)
  target_compile_options(ttf_build_flags INTERFACE /W4 /utf-8)
else()
  target_compile_options(ttf_build_flags INTERFACE -Wall -Wextra -Wpedantic)
  if(TTF_ENABLE_ASAN)
    target_compile_options(ttf_build_flags INTERFACE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(ttf_build_flags INTERFACE -fsanitize=address)
  endif()
  if(TTF_ENABLE_UBSAN)
    target_compile_options(ttf_build_flags INTERFACE -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(ttf_build_flags INTERFACE -fsanitize=undefined)
  endif()
endif()
foreach(KIND static shared)
  string(TOUPPER "${KIND}" LIBRARY_KIND)
  add_library(tts_text_frontend_${KIND} ${LIBRARY_KIND} ${TTF_ALL_SOURCES})
  target_include_directories(tts_text_frontend_${KIND}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src/core)
  target_compile_definitions(tts_text_frontend_${KIND} PRIVATE TTF_BUILDING TTF_DEFAULT_RESOURCE_ROOT="${TTF_DEFAULT_RESOURCE_ROOT}")
  target_link_libraries(tts_text_frontend_${KIND} PRIVATE ttf_build_flags)
  set_target_properties(tts_text_frontend_${KIND} PROPERTIES OUTPUT_NAME tts_text_frontend C_VISIBILITY_PRESET hidden)
endforeach()
target_compile_definitions(tts_text_frontend_shared PUBLIC TTF_SHARED)
set_target_properties(tts_text_frontend_shared PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
if(WIN32)
  set_target_properties(tts_text_frontend_static PROPERTIES OUTPUT_NAME tts_text_frontend_static)
endif()
add_library(tts_text_frontend::static ALIAS tts_text_frontend_static)
add_library(tts_text_frontend::shared ALIAS tts_text_frontend_shared)
add_executable(ttf tools/cli/main.c)
target_link_libraries(ttf PRIVATE tts_text_frontend_static ttf_build_flags)
add_executable(ttf_bench tools/benchmark/main.c)
target_link_libraries(ttf_bench PRIVATE tts_text_frontend_static ttf_build_flags)
add_executable(ttf_example examples/basic.c)
target_link_libraries(ttf_example PRIVATE tts_text_frontend_static ttf_build_flags)

if(BUILD_TESTING)
  add_executable(ttf_tests tests/integration/test_frontend.c)
  target_link_libraries(ttf_tests PRIVATE tts_text_frontend_shared ttf_build_flags)
  target_compile_definitions(ttf_tests PRIVATE TTF_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests" TTF_RESOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources")
  foreach(CATEGORY ko en code-mixed foreign-pronunciation streaming domain customer)
    add_test(NAME golden_${CATEGORY} COMMAND ttf_tests golden ${CATEGORY})
  endforeach()
  add_test(NAME api_and_tokenizer COMMAND ttf_tests api)
  add_test(NAME language_pack_extension COMMAND ttf_tests extension)
  add_test(NAME resource_and_lexicon_priority COMMAND ttf_tests resources)
  add_executable(ttf_test_multiword tests/unit/test_multiword_api.c)
  target_link_libraries(ttf_test_multiword PRIVATE tts_text_frontend_shared ttf_build_flags)
  add_test(NAME multiword_lexicon_api COMMAND ttf_test_multiword ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_rule_index tests/unit/test_rule_index.c)
  target_link_libraries(ttf_test_rule_index PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME rule_index_with_large_lexicon COMMAND ttf_test_rule_index ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_test(NAME arbitrary_chunk_streaming COMMAND ttf_tests streaming)
  add_executable(ttf_test_ko_pack tests/unit/test_ko_pack.c)
  target_link_libraries(ttf_test_ko_pack PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME korean_pack_edges COMMAND ttf_test_ko_pack ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_random tests/streaming/test_random.c)
  target_link_libraries(ttf_test_random PRIVATE tts_text_frontend_static ttf_build_flags)
  target_compile_definitions(ttf_test_random PRIVATE TEST_RESOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources")
  add_test(NAME randomized_streaming COMMAND ttf_test_random)
  add_executable(ttf_test_boundaries tests/streaming/test_semantic_boundaries.c)
  target_link_libraries(ttf_test_boundaries PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME semantic_boundary_streaming COMMAND ttf_test_boundaries ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_markup tests/unit/test_protected_markup.c)
  target_link_libraries(ttf_test_markup PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME protected_markup_grammar COMMAND ttf_test_markup)
  add_executable(ttf_test_protected_limits tests/streaming/test_protected_limits.c)
  target_link_libraries(ttf_test_protected_limits PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME protected_stream_limits COMMAND ttf_test_protected_limits ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_protected_cache tests/streaming/test_protected_cache.c)
  target_link_libraries(ttf_test_protected_cache PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME protected_stream_cache COMMAND ttf_test_protected_cache ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  if(UNIX)
    find_package(Threads REQUIRED)
    add_executable(ttf_test_concurrent tests/integration/test_concurrent.c)
    target_link_libraries(ttf_test_concurrent PRIVATE tts_text_frontend_shared ttf_build_flags Threads::Threads)
    add_test(NAME concurrent_shared_engine COMMAND ttf_test_concurrent ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  endif()
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME english_date_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_english_dates.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME english_dates_v13_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources english_dates_v13.jsonl 20)
    add_test(NAME scaled_currency_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_scaled_currency.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME scaled_currency_v12_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources scaled_currency_v12.jsonl 16)
    # Offline, original fixtures only: external corpora are explicit local inputs.
    add_test(NAME external_benchmark_diagnostics COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_diagnose.py)
    add_test(NAME external_sproat_importer COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/benchmark/importers/sproat/test_import_sproat.py)
    add_test(NAME external_korean_importers COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_korean_importers.py)
    add_test(NAME external_cmudict_importer COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_cmudict.py
      --ttf $<TARGET_FILE:ttf> --resources ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME external_benchmark_evaluator COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_evaluator.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME external_gap_measurement_contracts COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_measurement_resources.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME lexicon_generated_resources COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/lexicon/manage.py build --check)
    add_test(NAME lexicon_source_validation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/test_lexicon_sources.py)
    add_test(NAME lexicon_inventory_reachability COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_lexicon_inventory.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME lexicon_coverage_report COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_lexicon_coverage.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME lexicon_v10_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources lexicon_v10.jsonl 40)
    add_test(NAME english_clock_references COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_english_clock_reference.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME range_semantics COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_range_semantics.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME shared_scale_ranges COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_shared_scale_ranges.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME english_time_range_references COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_english_time_range_reference.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME reference_integrity COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/evaluation/audit_references.py --self-test)
    add_test(NAME cli_stream_json COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cli_json.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME benchmark_smoke COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_benchmark.py
      $<TARGET_FILE:ttf_bench> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME evaluation_reports COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_evaluation.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME evaluation_comparison COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_compare.py)
    add_test(NAME domain_text_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME compound_amount_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_compound_amounts.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME context_language_routes COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_context_routes.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME advanced_text_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources advanced_cases.jsonl 30)
    add_test(NAME content_context_and_lexicon COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_content_context.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME labeled_financial_identifiers COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_identifiers.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME financial_quantity_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_financial_quantities.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME protected_text_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources protected_cases.jsonl 40)
    add_test(NAME protected_text_integration COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_protected.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    foreach(CORPUS content_v7 finance_ranges_v7)
      if(CORPUS STREQUAL "content_v7")
        set(CASE_COUNT 40)
      else()
        set(CASE_COUNT 24)
      endif()
      add_test(NAME ${CORPUS}_evaluation COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CORPUS}.jsonl ${CASE_COUNT})
      add_test(NAME ${CORPUS}_integration COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_${CORPUS}.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endforeach()
    add_test(NAME multiword_v8_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources multiword_v8.jsonl 30)
    add_test(NAME multiword_v8_integration COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_multiword.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME multiword_boundary_guards COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_multiword_guards.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME english_date_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_english_dates.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME scaled_currency_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_scaled_currency.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_executable(ttf_web_abi tests/web/abi_layout.c)
      target_link_libraries(ttf_web_abi PRIVATE tts_text_frontend_static ttf_build_flags)
      add_test(NAME web_workbench COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/web/test_workbench.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE:ttf> $<TARGET_FILE:ttf_web_abi>)
    endif()
    add_test(NAME compiled_resources COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_compiled_resources.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/tools/resource_compiler/compile_resources.py
      ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/domain.tsv)
  endif()
  add_test(NAME cli_normalize COMMAND ttf normalize "Apple은 사과입니다.")
  set_tests_properties(cli_normalize PROPERTIES PASS_REGULAR_EXPRESSION "애플은 사과입니다.")
  add_test(NAME cli_inspect COMMAND ttf inspect "OpenAI가 발표했습니다.")
  set_tests_properties(cli_inspect PROPERTIES PASS_REGULAR_EXPRESSION "source=en-US.*context=ko-KR")
endif()
install(TARGETS tts_text_frontend_static tts_text_frontend_shared ttf ttf_bench
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY resources/ DESTINATION ${CMAKE_INSTALL_DATADIR}/tts-text-frontend/resources)
