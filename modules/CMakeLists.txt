# Scan for modules and create targets
file(GLOB CHILDREN RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

foreach(MOD_NAME ${CHILDREN})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${MOD_NAME} AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MOD_NAME}/Module)
        
        # Create an option to enable/disable this module (Default ON)
        option(ENABLE_MODULE_${MOD_NAME} "Enable module ${MOD_NAME}" ON)

        if(ENABLE_MODULE_${MOD_NAME})
            file(GLOB MOD_SOURCES "${MOD_NAME}/*.c" "${MOD_NAME}/*.cc")
            
            # Option to build as static (built-in) or shared (module)
            # Default to Shared (OFF) unless on iPod where we might want static by default
            option(MODULE_${MOD_NAME}_STATIC "Build ${MOD_NAME} statically linked" OFF)

            if(MODULE_${MOD_NAME}_STATIC)
                # Static build: Compile as static lib and link to main executable
                add_library(${MOD_NAME} STATIC ${MOD_SOURCES})
                target_link_libraries(podzilla ${MOD_NAME})
            else()
                # Dynamic build: Compile as MODULE
                add_library(${MOD_NAME} MODULE ${MOD_SOURCES})
                
                if(IPOD)
                    # On iPod, modules are relocatable objects (.mod.o)
                    set_target_properties(${MOD_NAME} PROPERTIES SUFFIX ".mod.o")
                    # CMake defaults to -shared for MODULE, but for .mod.o we usually want -r (relocatable)
                    # This requires specific linker flags override for this target if using standard GCC
                    target_link_options(${MOD_NAME} PRIVATE "-Wl,-r")
                else()
                    # On Desktop, standard .so
                    set_target_properties(${MOD_NAME} PROPERTIES SUFFIX ".so")
                endif()

                install(TARGETS ${MOD_NAME} DESTINATION usr/lib/${MOD_NAME})
            endif()

            # Common settings
            target_include_directories(${MOD_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
            target_compile_definitions(${MOD_NAME} PRIVATE 
                __PZ_BUILTIN_MODULE 
                __PZ_MODULE_NAME="${MOD_NAME}" 
                PZ_MOD
            )
        endif()
    endif()
endforeach()