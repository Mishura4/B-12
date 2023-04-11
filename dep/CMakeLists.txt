
if ((${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" AND "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC"))
	set(CMAKE_C_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING "Clang-cl C fix")
endif()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/fmt)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/boost_pfr)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/magic_enum)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/DPP)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/expected-lite)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libpng)

configure_file(libpng/scripts/pnglibconf.h.prebuilt libpng/pnglibconf.h COPYONLY)

include(FetchContent)

if(WIN32 AND NOT MINGW)
	if (NOT WINDOWS_32_BIT)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/bin/zlib1.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/bin/libcrypto-1_1-x64.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/bin/libssl-1_1-x64.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/bin/libsodium.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/bin/opus.dll" "${CMAKE_BINARY_DIR}" COPYONLY)

		message(warning, ${B12_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_BINARY_DIR})
	else()
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/32/bin/zlib1.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/32/bin/libcrypto-1_1.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/32/bin/libssl-1_1.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/32/bin/libsodium.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
		configure_file("${CMAKE_CURRENT_LIST_DIR}/DPP/win32/32/bin/opus.dll" "${CMAKE_BINARY_DIR}" COPYONLY)
	endif()
endif()

project(sqlite3
        LANGUAGES C)

add_library(sqlite3 STATIC
        ${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3.c
        ${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3.h)

set_target_properties(sqlite3 PROPERTIES LINKER_LANGUAGE C)

if(MSVC)
	target_compile_options(sqlite3
		PRIVATE
			/FI ${CMAKE_CURRENT_LIST_DIR}/cmake_definitions.h)
else()
	target_compile_options(sqlite3
		PRIVATE
			-include ${CMAKE_CURRENT_LIST_DIR}/cmake_definitions.h)
endif()

target_include_directories(sqlite3
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/sqlite/
)

if (UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PRIVATE Threads::Threads ${CMAKE_DL_LIBS} X11)
endif ()

install(TARGETS sqlite3
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
