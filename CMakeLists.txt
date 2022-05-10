# Set the minimum version of CMake that can be used
# To find the cmake version run
# $ cmake --version
cmake_minimum_required(VERSION 3.5)

# Set the project name
project (main)

# Create a sources variable with a link to all cpp files to compile
set(SOURCES
    main.c
    src/input_mgr.c
    src/mac_net.c
    src/output_mgr.c
    src/packet_frag.c
    src/pbuf.c
    src/radio_card.c
    src/radio_phy.c
    src/rf_tmr.c
)

# Add an executable with the above sources
add_executable(main ${SOURCES})

# Set the directories that should be included in the build command for this target
# when running g++ these will be included as -I/directory/path/
target_include_directories(main
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/inc
)
