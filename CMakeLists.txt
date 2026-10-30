cmake_minimum_required(VERSION 3.16)
project(tts_text_frontend VERSION 0.20.0 LANGUAGES C)
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
    if(EXISTS "${PACK_DIR}/validate.c")
      string(APPEND TTF_REGISTRY_DECLARATIONS "extern const ttf_resource_validator_t ttf_resource_validator_${PACK_SYMBOL};\n")
      string(APPEND TTF_REGISTRY_CALLS "    status = ttf_engine_register_resource_validator(e, &ttf_resource_validator_${PACK_SYMBOL});\n    if (status != TTF_OK) return status;\n")
    endif()
  endif()
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/builtin_registry.c"
  "#include \"internal.h\"\n#include \"tts_text_frontend/ttf_resource_validation.h\"\n${TTF_REGISTRY_DECLARATIONS}\nttf_status_t ttf_register_builtins(ttf_engine_t *e) {\n    ttf_status_t status = TTF_OK;\n${TTF_REGISTRY_CALLS}    return status;\n}\n")
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
  add_executable(ttf_test_numeric_width tests/streaming/test_numeric_width.c)
  target_link_libraries(ttf_test_numeric_width PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME numeric_width_native COMMAND ttf_test_numeric_width ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  set_tests_properties(numeric_width_native PROPERTIES TIMEOUT 60)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/cjk-lazy-resources)
  add_executable(ttf_test_cjk_lazy tests/unit/test_cjk_lazy.c)
  target_link_libraries(ttf_test_cjk_lazy PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME cjk_lazy_resources COMMAND ttf_test_cjk_lazy
    ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/cjk-lazy-resources)
  add_executable(ttf_test_cjk_stream tests/streaming/test_cjk_stream.c)
  target_link_libraries(ttf_test_cjk_stream PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME cjk_streaming_native COMMAND ttf_test_cjk_stream ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_cjk_index tests/unit/test_cjk_index.c)
  target_link_libraries(ttf_test_cjk_index PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME cjk_word_index COMMAND ttf_test_cjk_index ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_cjk_options tests/unit/test_cjk_options.c)
  target_link_libraries(ttf_test_cjk_options PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME cjk_options_abi COMMAND ttf_test_cjk_options ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_context_cache tests/unit/test_context_cache.c)
  target_link_libraries(ttf_test_context_cache PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME context_score_cache COMMAND ttf_test_context_cache ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_phone_scan tests/unit/test_phone_scan.c)
  target_link_libraries(ttf_test_phone_scan PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME phone_scanner_bounds COMMAND ttf_test_phone_scan ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  set_tests_properties(phone_scanner_bounds PROPERTIES TIMEOUT 60)
  if(TTF_ENABLE_ASAN OR TTF_ENABLE_UBSAN)
    # The 100k scaling gate runs in both ordinary compiler jobs. Instrumented
    # jobs exercise the same allocation and candidate paths at a smaller size.
    target_compile_definitions(ttf_test_phone_scan PRIVATE TTF_PHONE_SCAN_LARGE=30000)
    set_tests_properties(phone_scanner_bounds PROPERTIES TIMEOUT 120)
  endif()
  add_executable(ttf_tests tests/integration/test_frontend.c)
  target_link_libraries(ttf_tests PRIVATE tts_text_frontend_shared ttf_build_flags)
  target_compile_definitions(ttf_tests PRIVATE TTF_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests" TTF_RESOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources")
  foreach(CATEGORY ko en code-mixed foreign-pronunciation streaming domain customer)
    add_test(NAME golden_${CATEGORY} COMMAND ttf_tests golden ${CATEGORY})
  endforeach()
  add_test(NAME api_and_tokenizer COMMAND ttf_tests api)
  add_test(NAME language_pack_extension COMMAND ttf_tests extension)
  add_test(NAME resource_and_lexicon_priority COMMAND ttf_tests resources)
  add_executable(ttf_test_resource_validation tests/unit/test_resource_validation.c)
  target_link_libraries(ttf_test_resource_validation PRIVATE tts_text_frontend_static ttf_build_flags)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resource-validation-test")
  add_test(NAME resource_semantic_validation COMMAND ttf_test_resource_validation
    ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resource-validation-test)
  add_executable(ttf_test_multiword tests/unit/test_multiword_api.c)
  target_link_libraries(ttf_test_multiword PRIVATE tts_text_frontend_shared ttf_build_flags)
  add_test(NAME multiword_lexicon_api COMMAND ttf_test_multiword ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  add_executable(ttf_test_pronunciation_trace tests/unit/test_pronunciation_trace.c)
  target_link_libraries(ttf_test_pronunciation_trace PRIVATE tts_text_frontend_static ttf_build_flags)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pronunciation-trace-test")
  add_test(NAME detailed_pronunciation_trace COMMAND ttf_test_pronunciation_trace
    ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/pronunciation-trace-test)
  add_executable(ttf_test_pronunciation_provenance tests/unit/test_pronunciation_provenance.c)
  target_link_libraries(ttf_test_pronunciation_provenance PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME pronunciation_provenance_protocol COMMAND ttf_test_pronunciation_provenance ${CMAKE_CURRENT_SOURCE_DIR}/resources)
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
  add_executable(ttf_test_verbalize_fallback tests/unit/test_verbalize_fallback.c)
  target_link_libraries(ttf_test_verbalize_fallback PRIVATE tts_text_frontend_static ttf_build_flags)
  add_test(NAME verbalize_fallback COMMAND ttf_test_verbalize_fallback ${CMAKE_CURRENT_SOURCE_DIR}/resources)
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
    target_link_libraries(ttf_test_pronunciation_trace PRIVATE Threads::Threads)
    target_compile_definitions(ttf_test_pronunciation_trace PRIVATE TTF_TEST_HAVE_THREADS=1)
    target_link_libraries(ttf_test_context_cache PRIVATE Threads::Threads)
    target_compile_definitions(ttf_test_context_cache PRIVATE TTF_TEST_HAVE_THREADS=1)
    add_test(NAME concurrent_shared_engine COMMAND ttf_test_concurrent ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_executable(ttf_test_request_dict tests/unit/test_request_dict.c)
    target_link_libraries(ttf_test_request_dict PRIVATE tts_text_frontend_static ttf_build_flags Threads::Threads)
    target_compile_definitions(ttf_test_request_dict PRIVATE TTF_TEST_HAVE_THREADS=1)
    add_test(NAME request_dictionary COMMAND ttf_test_request_dict ${CMAKE_CURRENT_SOURCE_DIR}/resources)
  endif()
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cjk_generated_resources COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/cjk/generate.py build
      --unihan ${CMAKE_CURRENT_SOURCE_DIR}/data/cjk
      --out ${CMAKE_CURRENT_SOURCE_DIR}/resources/languages/ko-KR/cjk --check)
    add_test(NAME tn_improvement_harness COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/quality/test_tn_cycle.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME user_pattern_contracts COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_user_patterns.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME large_dictionary COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_large_dictionary.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME user_patterns_v20_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_patterns_v20.jsonl 51)
    add_test(NAME speech_final_gate_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources speech_final_gate_v21.jsonl 40)
    add_test(NAME letters_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources letters_v21.jsonl 26)
    add_test(NAME electronic_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources electronic_v21.jsonl 34)
    add_test(NAME math_hashtag_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources math_hashtag_v21.jsonl 37)
    add_test(NAME counter_band_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources counter_band_v21.jsonl 25)
    add_test(NAME floor_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources floor_v21.jsonl 19)
    add_test(NAME letters_paths_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources letters_paths_v21.jsonl 20)
    add_test(NAME letter_number_sign_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources letter_number_sign_v21.jsonl 17)
    add_test(NAME particle_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources particle_v21.jsonl 20)
    add_test(NAME calendar_alias_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources calendar_alias_v21.jsonl 72)
    add_test(NAME abbreviation_dotted_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources abbreviation_dotted_v21.jsonl 30)
    add_test(NAME abbreviation_dotted_resources COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_abbreviation_dotted_resources.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME speech_character_width_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources speech_character_width_v21.jsonl 30)
    add_test(NAME speech_character_resources COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_speech_character_resources.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME numeric_width_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources numeric_width_v22.jsonl 30)
    add_test(NAME typed_controls_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources typed_controls_v22.jsonl 67)
    add_test(NAME typed_controls_errors COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_typed_controls_errors.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME grouped_controls_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources grouped_controls_v22.jsonl 30)
    add_test(NAME dictionary_controls COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_dictionary_controls.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME grouped_controls_errors COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grouped_controls_errors.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME particle_table_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources particle_table_v21.jsonl 72)
    add_test(NAME mixed_case_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources mixed_case_v21.jsonl 20)
    add_test(NAME phone_service_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources phone_service_v21.jsonl 17)
    add_test(NAME slash_letters_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources slash_letters_v21.jsonl 13)
    add_test(NAME clock_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources clock_v21.jsonl 13)
    add_test(NAME slash_date_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources slash_date_v21.jsonl 16)
    add_test(NAME range_mark_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources range_mark_v21.jsonl 15)
    add_test(NAME multi_range_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources multi_range_v21.jsonl 11)
    add_test(NAME handle_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources handle_v21.jsonl 11)
    add_test(NAME year_cue_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources year_cue_v21.jsonl 45)
    add_test(NAME decade_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources decade_v21.jsonl 13)
    add_test(NAME initialism_speech_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources initialism_speech_v21.jsonl 14)
    add_test(NAME ko_acronym_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_acronym_v21.jsonl 20)
    add_test(NAME bare_abbreviation_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources bare_abbreviation_v21.jsonl 13)
    add_test(NAME measure_per_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources measure_per_v21.jsonl 9)
    add_test(NAME initial_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources initial_v21.jsonl 9)
    add_test(NAME unit_word_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources unit_word_v21.jsonl 16)
    add_test(NAME phone_cue_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources phone_cue_v21.jsonl 10)
    add_test(NAME unit_symbol_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources unit_symbol_v21.jsonl 16)
    add_test(NAME unit_sweep_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources unit_sweep_v21.jsonl 43)
    add_test(NAME currency_sweep_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources currency_sweep_v21.jsonl 45)
    add_test(NAME symbol_sweep_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources symbol_sweep_v21.jsonl 39)
    add_test(NAME numeral_char_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources numeral_char_v21.jsonl 29)
    add_test(NAME leading_zero_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources leading_zero_v21.jsonl 15)
    add_test(NAME unreadable_read_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources unreadable_read_v21.jsonl 13)
    add_test(NAME cctld_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources cctld_v21.jsonl 9)
    add_test(NAME ko_counter_native_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_counter_native_v21.jsonl 29)
    add_test(NAME ko_identifier_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_identifier_v21.jsonl 12)
    add_test(NAME ko_counter_native2_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_counter_native2_v21.jsonl 32)
    add_test(NAME ko_counter_native3_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_counter_native3_v21.jsonl 26)
    add_test(NAME bracket_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources bracket_v21.jsonl 11)
    add_test(NAME unknown_foreign_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources unknown_foreign_v21.jsonl 15)
    add_test(NAME decorative_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources decorative_v21.jsonl 13)
    add_test(NAME initialism_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources initialism_v21.jsonl 15)
    add_test(NAME code_word_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources code_word_v21.jsonl 13)
    add_test(NAME letter_label_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources letter_label_v21.jsonl 15)
    add_test(NAME oov_guess_regression_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources oov_guess_regression_v21.jsonl 28)
    add_test(NAME oov_guess_wide_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources oov_guess_wide_v21.jsonl 30)
    add_test(NAME oov_guess_neighbour_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
      oov_guess_neighbour_regression_v21.jsonl 17)
    add_test(NAME oov_guess_form_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
      oov_guess_form_regression_v21.jsonl 25)
    add_test(NAME oov_guess_neighbour2_v21_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
      oov_guess_neighbour2_v21.jsonl 24)
    add_test(NAME context_cache_compatibility COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_context_cache.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME phone_value_contracts COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_phone_contracts.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME phone_v18_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources phone_cases_v18.jsonl 55)
    add_test(NAME detailed_pronunciation_trace_cli COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_detailed_pronunciation_trace.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME pronunciation_source_trace COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_pronunciation_trace.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME unicode_grouping_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_unicode_grouping.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME korean_counter_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_korean_counters.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME korean_counters_v15_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources korean_counters_v15.jsonl 28)
    add_test(NAME pronunciation_alias_precedence COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_pronunciation_aliases.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME english_abbreviation_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_english_abbreviations.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME written_currency_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_written_currency.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME english_forms_v14_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources english_forms_v14.jsonl 24)
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
    add_test(NAME counted_noun_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources counted_noun_v22.jsonl 32)
    add_test(NAME break_controls_errors COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_break_errors.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME mark_controls COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_mark_control.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME age_break_streaming COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_age_break.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endif()
    add_test(NAME ko_spacing_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_spacing_v22.jsonl 30)
    add_test(NAME ko_spacing_audit COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_spacing_audit.py)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME ko_spacing_streaming COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_ko_spacing.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endif()
    add_test(NAME phone_zero_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources phone_zero_v22.jsonl 53)
    add_test(NAME identifier_fallback_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources identifier_fallback_v22.jsonl 19)
    add_test(NAME en_ref_fixes_v42_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_ref_fixes_v42.jsonl 20)
    add_test(NAME ko_pin_cues_v41_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_pin_cues_v41.jsonl 12)
    add_test(NAME ko_beon_count_v40_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_beon_count_v40.jsonl 16)
    add_test(NAME user_eval1_v81_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_eval1_v81.jsonl 19)
    add_test(NAME en_real_v94_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_real_v94.jsonl 23)
    add_test(NAME v2_four_v95_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources v2_four_v95.jsonl 42)
    add_test(NAME owner_dec_v96_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources owner_dec_v96.jsonl 15)
    add_test(NAME en_ja_dev_v97_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_ja_dev_v97.jsonl 39)
    add_test(NAME vi_th_dev_v98_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_th_dev_v98.jsonl 38)
    add_test(NAME en_listen_v99_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_listen_v99.jsonl 13)
    add_test(NAME en_owner_dec_v100_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_owner_dec_v100.jsonl 13)
    add_test(NAME ar_pack1_v101_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_pack1_v101.jsonl 27)
    add_test(NAME ar_pack2_v110_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_pack2_v110.jsonl 16)
    add_test(NAME ar_pack3_v111_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_pack3_v111.jsonl 6)
    add_test(NAME ko_v3_fixes_v113_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_v3_fixes_v113.jsonl 6)
    add_test(NAME ko_owner_dec_v114_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_owner_dec_v114.jsonl 10)
    add_test(NAME ko_superscript_v115_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_superscript_v115.jsonl 7)
    add_test(NAME ko_approx_unit_v116_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_approx_unit_v116.jsonl 4)
    add_test(NAME ko_styled_accent_v117_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_styled_accent_v117.jsonl 30)
    add_test(NAME ko_both_weak_v118_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_both_weak_v118.jsonl 26)
    add_test(NAME superscript_power_v119_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources superscript_power_v119.jsonl 7)
    add_test(NAME cjk_declared_v120_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources cjk_declared_v120.jsonl 16)
    add_test(NAME ko_address_v121_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_address_v121.jsonl 14)
    add_test(NAME ko_floor_cjk_v122_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_floor_cjk_v122.jsonl 15)
    add_test(NAME ar_controls_v123_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_controls_v123.jsonl 5)
    add_test(NAME ar_controls2_v124_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_controls2_v124.jsonl 21)
    add_test(NAME ar_v3fix_v125_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_v3fix_v125.jsonl 44)
    add_test(NAME ar_v1test_v126_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_v1test_v126.jsonl 23)
    add_test(NAME ar_v4fix_v127_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ar_v4fix_v127.jsonl 18)
    add_test(NAME foreign_sentence_v128_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources foreign_sentence_v128.jsonl 19)
    add_test(NAME ko_english_v129_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_english_v129.jsonl 18)
    add_test(NAME ko_song_titles_v130_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_song_titles_v130.jsonl 23)
    add_test(NAME tn_rounds_v131_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources tn_rounds_v131.jsonl 27)
    add_test(NAME tn_rounds_v132_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources tn_rounds_v132.jsonl 4)
    add_test(NAME tn_rounds_v133_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources tn_rounds_v133.jsonl 11)
    add_test(NAME tn_rounds_v134_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources tn_rounds_v134.jsonl 9)
    add_test(NAME tn_rounds_v135_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources tn_rounds_v135.jsonl 12)
    add_test(NAME tn_rounds_v136_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources tn_rounds_v136.jsonl 22)
    add_test(NAME tn_rounds_v137_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources tn_rounds_v137.jsonl 6)
    add_test(NAME th_pack4_v93_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources th_pack4_v93.jsonl 28)
    add_test(NAME control_packs_v92_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources control_packs_v92.jsonl 35)
    add_test(NAME th_pack3_v91_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources th_pack3_v91.jsonl 20)
    add_test(NAME th_pack2_v90_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources th_pack2_v90.jsonl 13)
    add_test(NAME th_pack1_v89_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources th_pack1_v89.jsonl 24)
    add_test(NAME user_eval7_v88_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_eval7_v88.jsonl 20)
    add_test(NAME vi_parity_v87_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_parity_v87.jsonl 19)
    add_test(NAME user_eval6_v86_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_eval6_v86.jsonl 18)
    add_test(NAME user_eval5_v85_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_eval5_v85.jsonl 28)
    add_test(NAME user_eval4_v84_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_eval4_v84.jsonl 22)
    add_test(NAME user_eval3_v83_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_eval3_v83.jsonl 19)
    add_test(NAME user_eval2_v82_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources user_eval2_v82.jsonl 27)
    add_test(NAME vi_prod_v80_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_prod_v80.jsonl 9)
    add_test(NAME vi_pack6_v79_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_pack6_v79.jsonl 9)
    add_test(NAME vi_pack5_v78_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_pack5_v78.jsonl 15)
    add_test(NAME vi_pack4_v77_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_pack4_v77.jsonl 22)
    add_test(NAME vi_pack3_v76_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_pack3_v76.jsonl 11)
    add_test(NAME vi_pack2_v75_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_pack2_v75.jsonl 26)
    add_test(NAME vi_pack1_v74_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources vi_pack1_v74.jsonl 49)
    add_test(NAME mixed_lang2_v73_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources mixed_lang2_v73.jsonl 32)
    add_test(NAME mixed_lang_v72_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources mixed_lang_v72.jsonl 41)
    add_test(NAME zh_pack8_v71_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack8_v71.jsonl 27)
    add_test(NAME zh_pack7_v70_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack7_v70.jsonl 42)
    add_test(NAME zh_pack6_v69_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack6_v69.jsonl 32)
    add_test(NAME zh_pack5_v68_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack5_v68.jsonl 36)
    add_test(NAME zh_pack4_v67_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack4_v67.jsonl 35)
    add_test(NAME zh_pack3_v66_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack3_v66.jsonl 39)
    add_test(NAME zh_pack2_v65_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack2_v65.jsonl 52)
    add_test(NAME zh_pack_v64_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources zh_pack_v64.jsonl 54)
    add_test(NAME ja_tts_fixes_v63_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_tts_fixes_v63.jsonl 14)
    add_test(NAME ja_goro_v62_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_goro_v62.jsonl 9)
    add_test(NAME ko_phoneme_v61_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v61.jsonl 5)
    add_test(NAME ko_particle_title_v60_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_particle_title_v60.jsonl 13)
    add_test(NAME ko_phoneme_v59_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v59.jsonl 8)
    add_test(NAME ko_phoneme_v58_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v58.jsonl 26)
    add_test(NAME ko_phoneme_v57_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v57.jsonl 19)
    add_test(NAME ko_phoneme_v56_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v56.jsonl 12)
    add_test(NAME ko_phoneme_v55_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v55.jsonl 13)
    add_test(NAME ko_phoneme_v54_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v54.jsonl 35)
    add_test(NAME ko_phoneme_v53_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v53.jsonl 17)
    add_test(NAME ko_phoneme_v52_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v52.jsonl 31)
    add_test(NAME ko_phoneme_v51_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_phoneme_v51.jsonl 50)
    add_test(NAME ja_rest_v50_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_rest_v50.jsonl 49)
    add_test(NAME ja_gaps78_v49_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_gaps78_v49.jsonl 62)
    add_test(NAME ja_slash_dates_v48_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_slash_dates_v48.jsonl 17)
    add_test(NAME ja_scores_v47_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_scores_v47.jsonl 20)
    add_test(NAME ja_counters_v46_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_counters_v46.jsonl 48)
    add_test(NAME ja_gaps_v45_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_gaps_v45.jsonl 44)
    add_test(NAME ja_range_mark_v44_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_range_mark_v44.jsonl 12)
    add_test(NAME ja_rules_v43_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_rules_v43.jsonl 12)
    add_test(NAME ja_kanji_numerals_v39_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_kanji_numerals_v39.jsonl 37)
    add_test(NAME ja_bench_fixes_v38_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_bench_fixes_v38.jsonl 21)
    add_test(NAME ja_phase2_v37_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_phase2_v37.jsonl 21)
    add_test(NAME ja_pack_v36_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ja_pack_v36.jsonl 69)
    add_test(NAME ja_segments COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_ja_segments.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME enclosed_letters_v35_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources enclosed_letters_v35.jsonl 7)
    add_test(NAME en_article_reading_v34_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_article_reading_v34.jsonl 5)
    add_test(NAME ko_particle_reading_v33_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_particle_reading_v33.jsonl 10)
    add_test(NAME ko_bench_fixes_v32_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_bench_fixes_v32.jsonl 23)
    add_test(NAME ko_jev_fixes_v31_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_jev_fixes_v31.jsonl 20)
    add_test(NAME en_engconf_v30_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_engconf_v30.jsonl 23)
    add_test(NAME en_jev_fixes_v29_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_jev_fixes_v29.jsonl 12)
    add_test(NAME en_segment_context_v28_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_segment_context_v28.jsonl 7)
    add_test(NAME en_fallback_v27_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_fallback_v27.jsonl 23)
    add_test(NAME en_wetext_gaps_v26_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_wetext_gaps_v26.jsonl 35)
    add_test(NAME en_digits_left_v25_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_digits_left_v25.jsonl 19)
    add_test(NAME en_bare_years_v25_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_bare_years_v25.jsonl 11)
    add_test(NAME ko_owner_judgements_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_owner_judgements_v24.jsonl 27)
    add_test(NAME ko_bench_lexicon_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_bench_lexicon_v24.jsonl 9)
    add_test(NAME ko_model_names_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_model_names_v24.jsonl 11)
    add_test(NAME ko_beon_frequency_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_beon_frequency_v24.jsonl 22)
    add_test(NAME ko_dae_rank_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_dae_rank_v24.jsonl 25)
    add_test(NAME ko_proper_additions_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_proper_additions_v24.jsonl 1)
    add_test(NAME ko_stdict_loanwords_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_stdict_loanwords_v24.jsonl 15)
    add_test(NAME ko_hanja_stdict_words_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_hanja_stdict_words_v24.jsonl 17)
    add_test(NAME ko_acronym_words_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_acronym_words_v24.jsonl 18)
    add_test(NAME ko_unit_symbols_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_unit_symbols_v24.jsonl 27)
    add_test(NAME ko_hanja_stdict_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_hanja_stdict_v24.jsonl 15)
    add_test(NAME ko_hanja_rules_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_hanja_rules_v24.jsonl 20)
    add_test(NAME ko_gae_counters_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_gae_counters_v24.jsonl 12)
    add_test(NAME ko_event_dates_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_event_dates_v24.jsonl 30)
    add_test(NAME ko_dict_counters_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_dict_counters_v24.jsonl 15)
    add_test(NAME ko_counter_bae_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_counter_bae_v24.jsonl 10)
    add_test(NAME ko_english_words_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_english_words_v24.jsonl 13)
    add_test(NAME ko_place_names_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_place_names_v24.jsonl 10)
    add_test(NAME ko_join_control_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_join_control_v24.jsonl 6)
    add_test(NAME ko_written_fallback_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_written_fallback_v24.jsonl 29)
    add_test(NAME ko_numeral_words_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_numeral_words_v24.jsonl 10)
    add_test(NAME ko_bench_leaks_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_bench_leaks_v24.jsonl 34)
    add_test(NAME ko_written_particles_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_written_particles_v24.jsonl 3)
    add_test(NAME ko_jev_leaks3_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_jev_leaks3_v24.jsonl 6)
    add_test(NAME ko_short_labels_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_short_labels_v24.jsonl 10)
    add_test(NAME ko_jev_leaks2_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_jev_leaks2_v24.jsonl 17)
    add_test(NAME ko_jev_leaks_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_jev_leaks_v24.jsonl 10)
    add_test(NAME ko_digit_codes_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_digit_codes_v24.jsonl 7)
    add_test(NAME ko_list_break_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_list_break_v24.jsonl 15)
    add_test(NAME ko_digit_groups_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_digit_groups_v24.jsonl 20)
    add_test(NAME ko_number_labels_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_number_labels_v24.jsonl 16)
    add_test(NAME ko_alnum_models_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_alnum_models_v24.jsonl 23)
    add_test(NAME ko_control_ignore_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_control_ignore_v24.jsonl 7)
    add_test(NAME ko_unit_control_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_unit_control_v24.jsonl 12)
    add_test(NAME ko_score_math_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_score_math_v24.jsonl 23)
    add_test(NAME ko_number_identifiers_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_number_identifiers_v24.jsonl 36)
    add_test(NAME ko_latin_joiners_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_latin_joiners_v24.jsonl 11)
    add_test(NAME ko_brand_lexicon_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_brand_lexicon_v24.jsonl 22)
    add_test(NAME ko_unit_leaks_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_unit_leaks_v24.jsonl 32)
    add_test(NAME ko_apparel_size_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_apparel_size_v24.jsonl 22)
    add_test(NAME ko_range_reading_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_range_reading_v24.jsonl 49)
    add_test(NAME unicode_fallback_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources unicode_fallback_v24.jsonl 28)
    add_test(NAME matrix_language_v24_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources matrix_language_v24.jsonl 28)
    add_test(NAME en_number_dash_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_number_dash_v23.jsonl 5)
    add_test(NAME en_meridiem_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_meridiem_v23.jsonl 15)
    add_test(NAME ko_clothing_size_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_clothing_size_v23.jsonl 6)
    add_test(NAME ko_number_letters_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_number_letters_v23.jsonl 11)
    add_test(NAME ko_speech_leaks_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_speech_leaks_v23.jsonl 12)
    add_test(NAME ko_vehicle_plate_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_vehicle_plate_v23.jsonl 11)
    add_test(NAME ko_identifier_shapes_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_identifier_shapes_v23.jsonl 12)
    add_test(NAME break_controls_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources break_controls_v23.jsonl 13)
    add_test(NAME ko_spacing_option_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_spacing_option_v23.jsonl 10)
    add_test(NAME ko_reading_unit_spacing_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_reading_unit_spacing_v23.jsonl 15)
    add_test(NAME en_street_title_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_street_title_v23.jsonl 8)
    add_test(NAME en_caps_word_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_caps_word_v23.jsonl 10)
    add_test(NAME ko_covid_name_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_covid_name_v23.jsonl 4)
    add_test(NAME ko_password_label_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_password_label_v23.jsonl 7)
    add_test(NAME ko_card_number_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_card_number_v23.jsonl 5)
    add_test(NAME en_month_year_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_month_year_v23.jsonl 10)
    add_test(NAME en_roman_numeral_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_roman_numeral_v23.jsonl 19)
    add_test(NAME ko_alnum_identifier_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources ko_alnum_identifier_v23.jsonl 17)
    add_test(NAME en_speech_hyphen_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_speech_hyphen_v23.jsonl 19)
    add_test(NAME en_decade_zero_v23_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources en_decade_zero_v23.jsonl 11)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME phone_zero_streaming COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_phone_zero.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endif()
    add_test(NAME lexical_coverage_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources lexical_coverage_v22.jsonl 36)
    add_test(NAME lexical_candidate_collection COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_lexicon_candidates.py)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME lexical_coverage_streaming COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_lexical_coverage.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endif()
    add_test(NAME counter_frequency_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources counter_frequency_v22.jsonl 33)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME counter_frequency_streaming COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_counter_frequency.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endif()
    add_test(NAME korean_release_harness COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_korean_release.py)
    add_test(NAME english_sentence_harness COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_english_sentences.py)
    add_test(NAME korean_release_reference_validator COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_korean_reference_validator.py)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME korean_release_native COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark/test_korean_release_native.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endif()
    add_test(NAME korean_release_web COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/web/test_ko_reports.py)
    add_test(NAME english_quality_web COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/web/test_en_reports.py)
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
    add_test(NAME cmu_english_dictionary_option COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cmu_option.py
      $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
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
    add_test(NAME option_matrix COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_option_matrix.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    # Each check is a separate CLI run, so an instrumented build needs room.
    set_tests_properties(option_matrix PROPERTIES
      TIMEOUT $<IF:$<OR:$<BOOL:${TTF_ENABLE_ASAN}>,$<BOOL:${TTF_ENABLE_UBSAN}>>,1200,300>)
    # Uninstrumented builds sweep through the shared library (one engine per pass).
    add_test(NAME speakable_sweep COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_speakable.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
      $<$<NOT:$<OR:$<BOOL:${TTF_ENABLE_ASAN}>,$<BOOL:${TTF_ENABLE_UBSAN}>>>:$<TARGET_FILE:tts_text_frontend_shared>>)
    # About 6,000 separate CLI runs, so engine start-up dominates; the 표준국어대사전
    # loanword table (2026-10-05) adds about 10 ms to each start.
    set_tests_properties(speakable_sweep PROPERTIES
      TIMEOUT $<IF:$<OR:$<BOOL:${TTF_ENABLE_ASAN}>,$<BOOL:${TTF_ENABLE_UBSAN}>>,1800,600>)
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
    # Like the existing byte-split tests, ctypes runs in ordinary compiler jobs.
    # Instrumented CI uses the native executable for every immutable contract,
    # plus the C callback/index tests for ASan ownership and streaming checks.
    if(TTF_ENABLE_ASAN OR TTF_ENABLE_UBSAN)
      set(TTF_CJK_CONTRACT_BACKEND --cli $<TARGET_FILE:ttf>)
    else()
      set(TTF_CJK_CONTRACT_BACKEND --library $<TARGET_FILE:tts_text_frontend_shared>)
    endif()
    add_test(NAME cjk_reading_contracts COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cjk_reading.py
      ${TTF_CJK_CONTRACT_BACKEND}
      --resources ${CMAKE_CURRENT_SOURCE_DIR}/resources --stage 3
      --report ${CMAKE_CURRENT_BINARY_DIR}/cjk-contracts.json)
    if(NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME cjk_kana_edges COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cjk_kana_edges.py
      $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE:ttf>)
      add_test(NAME cjk_hanja_edges COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_cjk_hanja_edges.py
      $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE:ttf>)
      add_test(NAME cjk_arbitrary_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/streaming/test_cjk_splits.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    endif()
    add_test(NAME trace_relative_order COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_trace_order.py)
    add_test(NAME domain_text_evaluation COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/test_domain.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME compound_amount_values COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_compound_amounts.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME context_language_routes COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_context_routes.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_test(NAME unicode_fallback_files COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_unicode_fallback.py
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
    add_test(NAME shared_unit_korean_readings COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_unit_readings.py
      ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    if(UNIX AND NOT TTF_ENABLE_ASAN AND NOT TTF_ENABLE_UBSAN)
      add_test(NAME numeric_width_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_numeric_width.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
      add_test(NAME typed_controls_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_typed_controls.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
      add_test(NAME grouped_controls_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_grouped_controls.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
      add_test(NAME speech_character_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_speech_characters.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
      add_test(NAME abbreviation_dotted_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_abbreviation_dotted.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
      add_test(NAME calendar_alias_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_calendar_aliases.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
      add_test(NAME particle_table_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_particle_resources.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources)
      add_test(NAME user_pattern_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_user_patterns.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared> --native-only)
      add_test(NAME context_cache_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_context_cache.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME phone_arbitrary_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_phone_contracts.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME pronunciation_trace_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_pronunciation_trace.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME unicode_grouping_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_unicode_grouping.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME korean_counter_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_korean_counters.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME pronunciation_alias_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_pronunciation_aliases.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME english_abbreviation_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_english_abbreviations.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
      add_test(NAME written_currency_byte_splits COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_written_currency.py
        $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        --library $<TARGET_FILE:tts_text_frontend_shared>)
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
      add_test(NAME web_value_check COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/web/test_value_check.py)
      add_test(NAME web_workbench COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/web/test_workbench.py
        $<TARGET_FILE:tts_text_frontend_shared> ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE:ttf> $<TARGET_FILE:ttf_web_abi>)
    endif()
    add_test(NAME resource_validation_cli COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_resource_validation.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/resource_compiler/compile_resources.py)
    add_test(NAME resource_compiler_protocol COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_resource_compiler_protocol.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/resources
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/resource_compiler/compile_resources.py)
    add_test(NAME compiled_resources COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_compiled_resources.py
      $<TARGET_FILE:ttf> ${CMAKE_CURRENT_SOURCE_DIR}/tools/resource_compiler/compile_resources.py
      ${CMAKE_CURRENT_SOURCE_DIR}/resources ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/domain.tsv)
    set_tests_properties(compiled_resources external_cmudict_importer resource_validation_cli resource_compiler_protocol
      PROPERTIES ENVIRONMENT "TTF_CLI=$<TARGET_FILE:ttf>")
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

# Tests with a wall-clock limit (a first-line or 30k-numeral timeout) run alone
# when ctest runs tests in parallel (tools/ci/run.sh, 2026-10-06).
foreach(serial_test numeric_width_native phone_scanner_bounds context_score_cache context_cache_compatibility context_cache_byte_splits cli_stream_json content_context_and_lexicon detailed_pronunciation_trace_cli)
  if(TEST ${serial_test})
    set_tests_properties(${serial_test} PROPERTIES RUN_SERIAL TRUE)
  endif()
endforeach()
