cmake_minimum_required(VERSION 3.16)
# Matching Engine Journal & Replay — C++17, zero third-party deps.
#
# Targets:
#   phase1_tests     — vendored LOB + scenario loader
#   journal_tests    — CRC, records, writer/reader
#   (later) journal_run / journal_replay
project(MatchingEngineJournalReplay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mejr_core INTERFACE)
target_include_directories(mejr_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(mejr_core INTERFACE -Wall -Wextra -Wpedantic)

add_executable(phase1_tests tests/test_phase1_lob.cpp)
target_link_libraries(phase1_tests PRIVATE mejr_core)

add_executable(journal_tests tests/test_journal_record.cpp)
target_link_libraries(journal_tests PRIVATE mejr_core)

add_executable(identity_tests tests/test_identity.cpp)
target_link_libraries(identity_tests PRIVATE mejr_core)

add_executable(journal_run src/journal_run.cpp)
target_link_libraries(journal_run PRIVATE mejr_core)

add_executable(journal_replay src/journal_replay.cpp)
target_link_libraries(journal_replay PRIVATE mejr_core)

enable_testing()
add_test(NAME phase1_tests COMMAND phase1_tests ${CMAKE_SOURCE_DIR})
add_test(NAME journal_tests COMMAND journal_tests)
add_test(NAME identity_tests COMMAND identity_tests ${CMAKE_SOURCE_DIR})
