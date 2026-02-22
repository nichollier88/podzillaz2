cmake_minimum_required(VERSION 3.10)
project(podzilla2 C CXX)

# --- Options ---
option(IPOD "Build for iPod (Cross-compile)" OFF)
set(PZ_VERSION "2.0" CACHE STRING "Podzilla Version")

# --- Compiler Flags & Toolchain ---
if(IPOD)
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR arm)
    
    # Default cross compiler names if not provided
    if(NOT CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER arm-uclinux-elf-gcc)
    endif()
    if(NOT CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER arm-uclinux-elf-g++)
    endif()
    if(NOT CMAKE_OBJCOPY)
        set(CMAKE_OBJCOPY arm-uclinux-elf-objcopy)
    endif()

    add_compile_options(
        -O3 -fomit-frame-pointer -funroll-loops 
        -mapcs -mcpu=arm7tdmi -Wall
        -DIPOD
    )
    
    # Linker flags for uClinux/FLT
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-elf2flt -Wl,-whole-archive")
else()
    # Desktop / X11
    if(APPLE)
        add_compile_options(-dynamic)
    else()
        add_compile_options(-fPIC -DPIC)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-E")
    endif()
    add_definitions(-DVERSION="${PZ_VERSION}")
endif()

# Fix multiple definition errors in legacy code (GCC 10+ defaults to -fno-common)
add_compile_options(-fcommon)

# --- Dependencies ---

# Find TTK via ttk-config
set(TTK_SEARCH_PATHS 
    ${CMAKE_SOURCE_DIR}/ttk 
    ${CMAKE_SOURCE_DIR}/../ttk 
    ${CMAKE_SOURCE_DIR}/../../../libs/ttk
)

find_program(TTK_CONFIG_EXE NAMES ttk-config ttk-config-here HINTS ${TTKDIR} $ENV{TTKDIR} PATHS ${TTK_SEARCH_PATHS})

if(NOT TTK_CONFIG_EXE)
    message(FATAL_ERROR "ttk-config not found. Please set TTKDIR or ensure it is in PATH.")
endif()

if(IPOD)
    set(TTK_PLATFORM_FLAG "--ipod")
else()
    set(TTK_PLATFORM_FLAG "--x11")
endif()

# Get CFLAGS from ttk-config
execute_process(
    COMMAND ${TTK_CONFIG_EXE} ${TTK_PLATFORM_FLAG} --sdl --cflags
    OUTPUT_VARIABLE TTK_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
separate_arguments(TTK_CFLAGS UNIX_COMMAND "${TTK_CFLAGS}")
add_compile_options(${TTK_CFLAGS})

get_filename_component(TTK_DIR "${TTK_CONFIG_EXE}" DIRECTORY)
include_directories(${TTK_DIR})

# Get LIBS from ttk-config
execute_process(
    COMMAND ${TTK_CONFIG_EXE} ${TTK_PLATFORM_FLAG} --sdl --libs
    OUTPUT_VARIABLE TTK_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# separate_arguments(TTK_LIBS UNIX_COMMAND "${TTK_LIBS}")
# set(TTK_LIBS "-L/workspace/ttk/build/src;-lttk-SDL;-L/usr/lib/aarch64-linux-gnu;-lSDL;-lSDL_image;-L/sw/lib;-L/usr/local/lib;-llgpio")
set(TTK_LIBS "-L/workspace/ttk/build/src;-lttk-SDL;-L/usr/lib/aarch64-linux-gnu;-lSDL;-lSDL_image;-L/sw/lib;-L/usr/local/lib")
message("TTK_LIBS: ${TTK_LIBS}")

# uCdl (Dynamic Loader)
if(IPOD)
    set(UCDL_LIB "${CMAKE_SOURCE_DIR}/../ucdl/libuCdl.a")
endif()

# --- Include Directories ---
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    /sw/include
)
link_directories(/sw/lib)

# --- Subdirectories ---
add_subdirectory(core)
add_subdirectory(modules)

# --- Main Executable ---
# Links the core object library and any static modules
add_executable(podzilla $<TARGET_OBJECTS:pz_core>)

target_link_libraries(podzilla ${TTK_LIBS})

if(IPOD)
    target_link_libraries(podzilla ${UCDL_LIB} intl supc++ "-Wl,-no-whole-archive")
else()
    target_link_libraries(podzilla stdc++ dl m)
endif()

install(TARGETS podzilla DESTINATION bin)