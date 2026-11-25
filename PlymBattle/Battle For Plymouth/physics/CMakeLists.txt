cmake_minimum_required(VERSION 3.24)
project(PlymouthPhysics LANGUAGES C CXX)

set(PHYSX_ROOT_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/PhysX-106.1-physx-5.4.2/physx")
list(APPEND CMAKE_MODULE_PATH "${PHYSX_ROOT_DIR}/source/compiler/cmake/modules")

set(TARGET_BUILD_PLATFORM windows)
set(PUBLIC_RELEASE 1)
set(PLYMOUTH_CPU_ONLY ON)
set(PX_GENERATE_STATIC_LIBRARIES ON CACHE BOOL "" FORCE)
set(PX_BUILDSNIPPETS OFF CACHE BOOL "" FORCE)
set(PX_BUILDPVDRUNTIME OFF CACHE BOOL "" FORCE)
set(NV_USE_STATIC_WINCRT OFF CACHE BOOL "" FORCE)
set(NV_USE_DEBUG_WINCRT ON CACHE BOOL "" FORCE)
set(PX_OUTPUT_LIB_DIR "${CMAKE_BINARY_DIR}/output")
set(PX_OUTPUT_BIN_DIR "${CMAKE_BINARY_DIR}/output")

add_subdirectory("${PHYSX_ROOT_DIR}/source/compiler/cmake" sdk EXCLUDE_FROM_ALL)
add_custom_target(build_physx DEPENDS
    PhysX PhysXCommon PhysXFoundation PhysXCooking
    PhysXExtensions PhysXCharacterKinematic PhysXPvdSDK)