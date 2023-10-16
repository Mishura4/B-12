cmake_minimum_required(VERSION 3.16)

FUNCTION(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
	  LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

project(shion LANGUAGES CXX)

set(SHION_HEADERS
	containers/consteval_map.h
	io/io.h
	io/logger.h
	media/image/image.h
	traits/type_list.h
	traits/value_list.h
	utils/bit_mask.h
	utils/flexible_ptr.h
	utils/misc.h
	utils/observer_ptr.h
	utils/owned_resource.h
	utils/protected_ptr.h
	utils/reference.h
	utils/string_literal.h
	utils/variable_ref.h
	utils/visitor.h
	exception.h
	shion.h
	types.h
)

set(SHION_SOURCES
	media/image/image.cpp
)

list(TRANSFORM SHION_SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/src/)
list(TRANSFORM SHION_HEADERS PREPEND ${CMAKE_CURRENT_LIST_DIR}/include/shion/)

add_library(shion)

set_target_properties(shion
	PROPERTIES
		LINKER_LANGUAGE CXX
)

target_compile_features(shion PUBLIC cxx_std_20)

target_compile_definitions(shion PRIVATE SHION_BUILD)

target_sources(shion
PUBLIC
	${SHION_HEADERS}
PRIVATE
	${SHION_SOURCES}
)

target_include_directories(shion
PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/>
	$<INSTALL_INTERFACE:include/>
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/dep/libpng)

target_include_directories(shion PRIVATE dep/libpng)
target_include_directories(shion PRIVATE dep/CImg)
target_include_directories(shion PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/dep/libpng)

configure_file(dep/libpng/scripts/pnglibconf.h.prebuilt dep/libpng/pnglibconf.h COPYONLY)

target_link_libraries(shion PRIVATE png_static)
