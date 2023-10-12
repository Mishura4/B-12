cmake_minimum_required(VERSION 3.16)

FUNCTION(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
	  LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

set(BUILD_SHARED_LIBRARIES OFF)

project(B12
		LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>")
	add_link_options("$<$<COMPILE_LANGUAGE:CXX>:-lc++abi>")
endif()

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
 "src/proto_dpp.h")

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
target_link_libraries(B12 PUBLIC png_static)

if (UNIX)
	find_package(X11 REQUIRED)
	target_link_libraries(B12 PRIVATE ${X11_LIBRARIES})
	if (CMAKE_COMPILER_IS_GNUXX)
		target_link_libraries(B12 PUBLIC -static-libgcc -static-libstdc++)
	endif()
endif ()

target_compile_definitions(
	B12
	PUBLIC
		$<$<BOOL:${WIN32}>:_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING>
		$<$<CONFIG:Debug>:B12_DEBUG>
		$<$<CONFIG:Release>:B12_RELEASE>

		$<$<CONFIG:RelWithDebInfo>:B12_RELEASE>
		$<$<CONFIG:MinSizeRel>:B12_RELEASE>
)

if(MSVC)
	target_compile_options(B12 PRIVATE /W4 /wd4251 /utf-8)

	if (NOT WINDOWS_32_BIT)
		set(DPP_DEP_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/dep/DPP/win32/bin")
	else()
		set(DPP_DEP_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/dep/DPP/win32/32")
	endif()

	set(B12_DEPENDENCIES
			"${DPP_DEP_DIRECTORY}/libcrypto-1_1-x64.dll"
			"${DPP_DEP_DIRECTORY}/libsodium.dll"
			"${DPP_DEP_DIRECTORY}/libssl-1_1-x64.dll"
			"${DPP_DEP_DIRECTORY}/opus.dll"
			"${DPP_DEP_DIRECTORY}/zlib1.dll"
			$<TARGET_FILE:dpp>
			$<TARGET_FILE:fmt>
			$<TARGET_FILE:sqlite3>
			$<TARGET_FILE_DIR:B12>)

	add_custom_target(COPY_DEPENDENCIES ALL
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${B12_DEPENDENCIES}
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
