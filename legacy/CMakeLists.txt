cmake_minimum_required(VERSION 3.21)

# NPCAIAutoPatch.dll for the older CommonLibSSE line (Skyrim SE 1.5.97, AE up to 1.6.1170, VR).
# The same sources as the alandtse (NG) build in ../ng.cmake; NPCAIAUTOPATCH_LEGACY_COMMONLIB switches the few lines that differ.
project(NPCAIAutoPatchLegacy VERSION 0.4.0 LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME} NAME "NPCAIAutoPatch" AUTHOR "CageTV" SOURCES
    ../src/main.cpp
    ../src/Core.cpp
    ../src/Menu.cpp
    ../src/Lang.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME} PRIVATE NPCAIAUTOPATCH_LEGACY_COMMONLIB)
target_include_directories(${PROJECT_NAME} PRIVATE ../src ../include)
target_precompile_headers(${PROJECT_NAME} PRIVATE ../src/PCH.h)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/Z7>)
target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
