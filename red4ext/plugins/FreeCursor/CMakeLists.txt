cmake_minimum_required(VERSION 3.21)
project(FreeCursor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  RED4ext.SDK
  GIT_REPOSITORY https://github.com/WopsS/RED4ext.SDK.git
  GIT_TAG        ad7277714ad30d6885d7050c5ba24fa0102f6920
)
FetchContent_MakeAvailable(RED4ext.SDK)

add_library(FreeCursor SHARED
  src/dllmain.cpp
  src/window_hook.cpp
)

target_link_libraries(FreeCursor PRIVATE RED4ext::SDK)
target_compile_definitions(FreeCursor PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

set_target_properties(FreeCursor PROPERTIES OUTPUT_NAME "FreeCursor" SUFFIX ".dll")

# Ship debug symbols with the optimised build. Without a PDB, a crash dump that
# names FreeCursor.dll gives only a raw offset, which is unusable for diagnosis.
# /Zi does not change codegen; /OPT:REF,ICF restores the size folding that
# linking with /DEBUG would otherwise turn off.
target_compile_options(FreeCursor PRIVATE $<$<CONFIG:Release>:/Zi>)
target_link_options(FreeCursor PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF /MAP>)

# The .map turns a crash-dump offset into a function name with nothing but a
# text search, so a dump stays readable even where the .pdb has been lost.
