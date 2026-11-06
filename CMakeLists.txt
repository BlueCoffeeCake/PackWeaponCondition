cmake_minimum_required(VERSION 3.20)
project(PackWeaponCondition VERSION 0.2.0.0 LANGUAGES C CXX RC)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "PackWeaponCondition requires 64-bit Windows")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_library(minhook STATIC
  third_party/minhook/src/buffer.c
  third_party/minhook/src/hook.c
  third_party/minhook/src/trampoline.c
  third_party/minhook/src/hde/hde64.c)
target_include_directories(minhook PUBLIC third_party/minhook/include)
add_library(PackWeaponCondition SHARED native/PackWeaponCondition.cpp resources/version.rc)
target_compile_features(PackWeaponCondition PRIVATE cxx_std_17)
target_link_libraries(PackWeaponCondition PRIVATE minhook bcrypt user32)
if(MSVC)
  target_compile_options(PackWeaponCondition PRIVATE /O2 /GR-)
else()
  target_compile_options(PackWeaponCondition PRIVATE -O2 -fno-exceptions -fno-rtti)
endif()
option(PWC_BUILD_NATIVE_TESTS "Build isolated native policy harnesses" OFF)
if(PWC_BUILD_NATIVE_TESTS)
  enable_testing()
  add_executable(TestCondition native/TestCondition.cpp)
  target_compile_features(TestCondition PRIVATE cxx_std_17)
  target_link_libraries(TestCondition PRIVATE minhook bcrypt user32)
  add_test(NAME condition-policy COMMAND TestCondition)
  add_executable(TestEnvironment native/TestEnvironment.cpp)
  target_compile_features(TestEnvironment PRIVATE cxx_std_17)
  add_test(NAME environment-policy COMMAND TestEnvironment)
endif()
# TestHooks requires separately owned proprietary image fixtures. It is not a
# public CI target and no game/framework image is downloaded by this project.
