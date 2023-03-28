cmake_minimum_required(VERSION 3.16)

FUNCTION(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

set(FMT_INSTALL ON CACHE BOOL "Fmt installation" FORCE)
set(DPP_INSTALL ON CACHE BOOL "Dpp installation" FORCE)

project(B12
        LANGUAGES CXX)

add_executable(B12)

add_subdirectory(src/)

source_group(
    TREE
        ${CMAKE_CURRENT_LIST_DIR}/src/
    PREFIX
        "Source Files"
    FILES
        ${B12_SOURCES}
)

target_sources(B12
    PRIVATE
        ${B12_SOURCES}
)

set_target_properties(B12
    PROPERTIES
        LINKER_LANGUAGE CXX
)
target_compile_features(B12 PUBLIC cxx_std_20)

add_subdirectory(dep)

target_include_directories(B12 PRIVATE src/)
target_include_directories(B12 PRIVATE dep/shion/include)
target_include_directories(B12 PRIVATE dep/libpng)
target_include_directories(B12 PRIVATE dep/CImg)
target_include_directories(B12 PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/dep/libpng)

target_link_libraries(B12 PUBLIC fmt)
target_link_libraries(B12 PUBLIC sqlite3)
target_link_libraries(B12 PUBLIC dpp)
target_link_libraries(B12 PUBLIC boost_pfr)
target_link_libraries(B12 PUBLIC magic_enum)
target_link_libraries(B12 PUBLIC nonstd::expected-lite)
target_link_libraries(B12 PUBLIC zlibstatic)
target_link_libraries(B12 PUBLIC png_static)

target_compile_definitions(B12 PUBLIC _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

if(MSVC)
    target_compile_options(B12 PRIVATE /W4)

    if (NOT WINDOWS_32_BIT)
        set(DPP_DEP_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/dep/DPP/win32/bin")
    else()
        set(DPP_DEP_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/dep/DPP/win32/32")
    endif()

    add_custom_command(TARGET B12 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
	        "${DPP_DEP_DIRECTORY}/libcrypto-1_1-x64.dll"
	        "${DPP_DEP_DIRECTORY}/libsodium.dll"
	        "${DPP_DEP_DIRECTORY}/libssl-1_1-x64.dll"
	        "${DPP_DEP_DIRECTORY}/opus.dll"
	        "${DPP_DEP_DIRECTORY}/zlib1.dll"
            $<TARGET_FILE:dpp>
            $<TARGET_FILE:fmt>
            $<TARGET_FILE:sqlite3>
            $<TARGET_PDB_FILE:dpp>
            $<TARGET_PDB_FILE:fmt>
	        $<TARGET_FILE_DIR:B12>
    )
else()
  target_compile_options(B12 PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS B12
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# cl sqlite3.c -O2 -Oi -Ob2 -Gy -GL -DNDEBUG -DSQLITE_API=__declspec(dllexport) -link -dll -out:sqlite3.dll -LTCG