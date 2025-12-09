# modified by Orfeas Karachalios (2025 NCSR Demokritos, Greece)
# to add stub generation for Python bindings
# and restucture the build process

pybind11_add_module(ns3penv_gym_msg_py msg_py_binding.cc)

set_target_properties(ns3penv_gym_msg_py PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${BINDINGS_OUTPUT_DIR})

# Find the stub generator tool
find_program(PYBIND11_STUBGEN_EXECUTABLE pybind11-stubgen)

if(PYBIND11_STUBGEN_EXECUTABLE)
    message(STATUS "Found pybind11-stubgen: ${PYBIND11_STUBGEN_EXECUTABLE}")

    # Define a custom command that runs after the module is built
    # This command generates the .pyi file in the same directory as the module
    add_custom_command(
        TARGET ns3penv_gym_msg_py
        POST_BUILD
        COMMAND "PYTHONPATH=${BINDINGS_OUTPUT_DIR}" ${PYBIND11_STUBGEN_EXECUTABLE} -o ${BINDINGS_OUTPUT_DIR} ns3penv_gym_msg_py
        COMMENT "Generating pybind11 stub file for ns3penv_gym_msg_py"
        VERBATIM
    )
else()
    message(WARNING "pybind11-stubgen not found. Python stub file (.pyi) will not be generated.")
endif()

add_dependencies(${libns3penv} ns3penv_gym_msg_py)

