# Specify the required cmake version
cmake_minimum_required(VERSION 3.20)
cmake_policy(VERSION 3.20)

# Add CMake option to enable individual pin functions
option(mcp23017_lib_enable_individual_pin_functions "Enable individual pin functions" ON)

# Check to see if the individual pin functions should be enabled
if (mcp23017_lib_enable_individual_pin_functions)
    add_compile_definitions(${PROJECT_NAME} PRIVATE MCP23017_LIB_ENABLE_INDIVIDUAL_PIN_FUNCTIONS)
endif()

# Define the static library and all the associated files.
add_library(mcp23017_lib STATIC
    mcp23017_lib.c
)

# Set and enforce the C standards
set_target_properties(mcp23017_lib PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED On
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED On
)

# Link the needed libraries
target_link_libraries(mcp23017_lib
    pico_stdlib
    hardware_i2c
)

# Enable most warnings for the library
target_compile_options(mcp23017_lib PRIVATE -Wall -W -Wconversion -Wshadow -Wcast-qual -Wwrite-strings)

# Add optimizations based on the build level
if (CMAKE_BUILD_TYPE MATCHES Release)
    target_compile_options(mcp23017_lib PRIVATE -O2)
else()
    target_compile_options(mcp23017_lib PRIVATE -Og)
endif()
