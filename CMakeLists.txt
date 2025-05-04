cmake_minimum_required(VERSION 3.28)
project(Quiltis LANGUAGES CXX)

option(QUILTIS_FIND_SFML "Use find_package to find SFML" OFF)

if(QUILTIS_FIND_SFML)
  if(NOT BUILD_SHARED_LIBS)
    set(SFML_STATIC_LIBRARIES ON)
  endif()
  find_package(SFML 3 REQUIRED COMPONENTS Graphics)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(Quiltis quiltis.cpp)
add_library(Quiltis::Quiltis ALIAS Quiltis)

target_compile_features(Quiltis PRIVATE cxx_std_20)
set_property(TARGET Quiltis PROPERTY CXX_STANDARD 20)

target_include_directories(Quiltis PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(Quiltis PRIVATE QUILTIS_SHARED_LIB)
  set_target_properties(Quiltis PROPERTIES DEFINE_SYMBOL "QUILTIS_EXPORTS")
  set_target_properties(Quiltis PROPERTIES DEBUG_POSTFIX "_d")
endif()

set(IMGUI_SFML_PUBLIC_HEADERS
  ${PROJECT_SOURCE_DIR}/quiltis.hpp
)
set_target_properties(Quiltis PROPERTIES PUBLIC_HEADER "${QUILTIS_PUBLIC_HEADERS}")

target_link_libraries(Quiltis PUBLIC SFML::Graphics)

install(TARGETS Quiltis
  EXPORT Quiltis
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT Quiltis
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Quiltis
  NAMESPACE Quiltis::
)

install(FILES ${PROJECT_SOURCE_DIR}/cmake/QuiltisConfig.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Quiltis
)

# Stop configuration if being consumed by a higher level project
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

option(QUILTIS_BUILD_APP "Build the Quiltis app" ON)
if(QUILTIS_BUILD_APP)
  add_subdirectory(app)
endif()