check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options("-Wno-gcc-compat")
endif()

find_package(Boost)
include_directories(${Boost_INCLUDE_DIRS})
find_package(Python REQUIRED COMPONENTS Interpreter Development)
if (Python_FOUND)
    message(STATUS "Python include directories: ${Python_INCLUDE_DIRS}")
    message(STATUS "Python library directories: ${Python_LIBRARY_DIRS}")
endif()
find_package(pybind11 CONFIG REQUIRED)
include_directories(${pybind11_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
if (NOT PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config not found")
endif()

pkg_check_modules(PROTOBUF protobuf REQUIRED IMPORTED_TARGET)
if (PROTOBUF_FOUND)
    message(STATUS "Found Protobuf: ${PROTOBUF_VERSION}")
    message(STATUS "Protobuf include dirs: ${PROTOBUF_INCLUDE_DIRS}")
    message(STATUS "Protobuf library dirs: ${PROTOBUF_LIBRARY_DIRS}")
else()
    message(FATAL_ERROR "Protobuf not found")
endif()
include_directories(${PROTOBUF_INCLUDE_DIRS})
# Use protobuf libraries directly (avoid imported target for monolink)
link_directories(${PROTOBUF_LIBRARY_DIRS})

# Honor top-level --disable-warnings
set(NS3_WARNINGS_DISABLED OFF)
if(DEFINED NS3_WARNINGS AND NOT NS3_WARNINGS)
    set(NS3_WARNINGS_DISABLED ON)
elseif(DEFINED NS3_ENABLE_WARNINGS AND NOT NS3_ENABLE_WARNINGS)
    set(NS3_WARNINGS_DISABLED ON)
elseif(CMAKE_CXX_FLAGS MATCHES "(^| )-w( |$)")
    set(NS3_WARNINGS_DISABLED ON)
endif()
if(NS3_WARNINGS_DISABLED)
    message(STATUS "Disabling warnings (-w) for ns3penv and related targets")
endif()

# Compile proto files
add_library(proto-objects OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/proto/messages.proto")

set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto")

target_include_directories(proto-objects PUBLIC SYSTEM "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")

# protobuf_generate function is missing in some installations by package manager
check_function_exists(protobuf_generate protobuf_generate_exists)
if(${protobuf_generate_exists})
    message(STATUS "protobuf_generate function found")
else()
    message(STATUS "protobuf_generate function not found -> use a local copy from ${CMAKE_CURRENT_SOURCE_DIR}/protobuf-generate.cmake")
    include(${CMAKE_CURRENT_SOURCE_DIR}/protobuf-generate.cmake)
endif()

set(PROTO_GEN_CPP_DIR "${CMAKE_CURRENT_BINARY_DIR}/cppproto")

set(CPP_MODULE_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/model")

set(PROTO_GEN_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/pyproto")

set(PYTHON_PACKAGE_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/python/src/ns3env/proto")

find_program(PROTOC_EXECUTABLE protoc)
if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc executable not found. Please install protobuf-compiler")
endif()
message(STATUS "Found protoc executable: ${PROTOC_EXECUTABLE}")

find_program(MYPY_PLUGIN_EXECUTABLE protoc-gen-mypy)
if(NOT MYPY_PLUGIN_EXECUTABLE)
    message(FATAL_ERROR "mypy-protobuf plugin not found. Please install mypy-protobuf")
endif()
message(STATUS "Found mypy-protobuf plugin: ${MYPY_PLUGIN_EXECUTABLE}")

protobuf_generate(
        TARGET proto-objects
        IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto/" 
        LANGUAGE cpp
        PROTOC_EXE ${PROTOC_EXECUTABLE}
        PROTOC_OUT_DIR ${PROTO_GEN_CPP_DIR}
)

protobuf_generate(
        TARGET proto-objects
        IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/proto"
        LANGUAGE python
        PROTOC_OUT_DIR ${PROTO_GEN_PY_DIR}
        PROTOC_EXE ${PROTOC_EXECUTABLE}
        PLUGIN ${MYPY_PLUGIN_EXECUTABLE}
        PROTOC_OPTIONS "--mypy_out=${PROTO_GEN_PY_DIR}"
        PLUGIN_EXTENSIONS _pb2.pyi
)

add_custom_command(
    OUTPUT ${CPP_MODULE_OUTPUT_DIR}/messages.pb.cc
           ${CPP_MODULE_OUTPUT_DIR}/messages.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy ${PROTO_GEN_CPP_DIR}/messages.pb.cc ${CPP_MODULE_OUTPUT_DIR}/messages.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy ${PROTO_GEN_CPP_DIR}/messages.pb.h ${CPP_MODULE_OUTPUT_DIR}/messages.pb.h
    DEPENDS ${PROTO_GEN_CPP_DIR}/messages.pb.h
            ${PROTO_GEN_CPP_DIR}/messages.pb.cc
    COMMENT "Copying generated C++ protobuf files to model directory"
)

add_custom_target(copy_protobuf_cpp ALL
    DEPENDS ${CPP_MODULE_OUTPUT_DIR}/messages.pb.cc
            ${CPP_MODULE_OUTPUT_DIR}/messages.pb.h
)

add_custom_command(
    OUTPUT ${PYTHON_PACKAGE_PROTO_DIR}/messages_pb2.py
           ${PYTHON_PACKAGE_PROTO_DIR}/messages_pb2.pyi
    COMMAND ${CMAKE_COMMAND} -E copy ${PROTO_GEN_PY_DIR}/messages_pb2.py ${PYTHON_PACKAGE_PROTO_DIR}/messages_pb2.py
    COMMAND ${CMAKE_COMMAND} -E copy ${PROTO_GEN_PY_DIR}/messages_pb2.pyi ${PYTHON_PACKAGE_PROTO_DIR}/messages_pb2.pyi
    DEPENDS ${PROTO_GEN_PY_DIR}/messages_pb2.py ${PROTO_GEN_PY_DIR}/messages_pb2.pyi
    COMMENT "Copying generated Python protobuf files to python package directory"
)

add_custom_target(copy_protobuf_python ALL
    DEPENDS ${PYTHON_PACKAGE_PROTO_DIR}/messages_pb2.py
            ${PYTHON_PACKAGE_PROTO_DIR}/messages_pb2.pyi
)

set_source_files_properties(${CPP_MODULE_OUTPUT_DIR}/messages.pb.cc PROPERTIES COMPILE_FLAGS "-w")
set_source_files_properties(${CPP_MODULE_OUTPUT_DIR}/messages.pb.h PROPERTIES COMPILE_FLAGS "-w")

set(src_files
        model/ns3penv-gym-interface.cc
        model/ns3penv-gym-env.cc
        model/container.cc
        model/spaces.cc
        model/messages.pb.cc
)
set(header_files
        model/ns3penv-gym-interface.h
        model/ns3penv-gym-env.h
        model/ns3penv-gym-msg.h
        model/ns3penv-msg-interface.h
        model/ns3penv-semaphore.h
        model/container.h
        model/spaces.h
)

set(BINDINGS_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/python/src/ns3env")

build_lib(
        LIBNAME ns3penv
        SOURCE_FILES ${src_files}
        HEADER_FILES ${header_files}
        LIBRARIES_TO_LINK ${libcore} ${PROTOBUF_LIBRARIES}
)

# Ensure -w is appended after build_lib sets its own warning flags
if(NS3_WARNINGS_DISABLED)
    target_compile_options(proto-objects PRIVATE -w)
    target_compile_options(ns3penv PRIVATE -w)
endif()

add_dependencies(ns3penv proto-objects copy_protobuf_cpp copy_protobuf_python)

# Build Gym msg binding module
add_subdirectory(bindings)

