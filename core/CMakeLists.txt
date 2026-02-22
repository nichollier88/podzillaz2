file(GLOB CORE_SOURCES "*.c" "*.cc")

add_library(pz_core OBJECT ${CORE_SOURCES})

# Ensure position independent code if we are building shared modules against this
set_property(TARGET pz_core PROPERTY POSITION_INDEPENDENT_CODE ON)