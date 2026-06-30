cmake_minimum_required(VERSION 3.16)
project(oscad CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenCASCADE REQUIRED)
message(STATUS "OCCT version : ${OpenCASCADE_VERSION}")
message(STATUS "OCCT include : ${OpenCASCADE_INCLUDE_DIR}")

# Minimal toolkit set for Milestone 1. Linking the full ${OpenCASCADE_LIBRARIES}
# would drag in the DRAW test harness (TKDraw/TKXSDRAW/...) from libocct-draw-dev,
# which we neither have nor need. These are imported targets from the OCCT config.
set(OCCT_LIBS
  TKernel TKMath                              # foundation
  TKG2d TKG3d TKGeomBase TKBRep               # modeling-data
  TKGeomAlgo TKTopAlgo TKPrim                 # primitives + topo algos
  TKBO TKBool TKFillet TKOffset TKShHealing   # booleans / healing
  TKMesh                                      # tessellation (BRepMesh)
  TKXSBase TKDESTEP TKDESTL)                  # data exchange: STEP + STL

# Toolchain smoke test -- verifies OCCT build/link/export independent of the interpreter.
add_executable(smoke_occt tests/smoke_occt.cc)
target_include_directories(smoke_occt PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(smoke_occt PRIVATE ${OCCT_LIBS})

# The interpreter.
add_executable(oscad
  src/main.cc
  src/Lexer.cc
  src/Parser.cc
  src/Evaluator.cc
  src/Kernel.cc
  src/Export.cc)
target_include_directories(oscad PRIVATE ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(oscad PRIVATE ${OCCT_LIBS})
