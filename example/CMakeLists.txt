include(GenerateExportHeader)
find_package(spdlog CONFIG REQUIRED)

link_libraries(spdlog::spdlog cereal_dll)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(dummy_lib_base SHARED Base.cpp)
target_compile_features(dummy_lib_base PUBLIC cxx_std_20)
generate_export_header(dummy_lib_base)

add_library(dummy_lib_derived SHARED Derived.cpp)
generate_export_header(dummy_lib_derived)
target_link_libraries(dummy_lib_derived PUBLIC dummy_lib_base)

add_executable(cereal_serialization main.cpp)
target_link_libraries(cereal_serialization PRIVATE dummy_lib_derived)
