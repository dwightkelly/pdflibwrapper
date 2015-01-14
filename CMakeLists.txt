cmake_minimum_required(VERSION 3.0)
project(pdflibwrapper)
add_executable(wrappertest PDFLWrapper.cpp PDFLibWrapper.cpp WrapperTest.cpp)
set_target_properties(wrappertest  PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")

find_package(JPEG REQUIRED)
include_directories (${JPEG_INCLUDE_DIR})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

set(Boost_USE_MULTITHREAD ON) 
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost COMPONENTS thread REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Adobe PDFLibrary
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(APDFL_DEFS -DWIN_PLATFORM -DWIN_ENV -DPLATFORM="WinPlatform.h")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(APDFL_DEFS -DUNIX_PLATFORM -DUNIX_ENV -DPLATFORM="UnixPlatform.h")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(APDFL_DEFS -DMAC_PLATFORM -DMAC_ENV -DPLATFORM="MacPlatform.h")
endif()

add_definitions(-DADOBE_PDFL -DPRODUCT="Plugin.h" ${APDFL_DEFS})
set(PDFL_INCLUDE_DIRS "/Users/dkelly/Documents/misc/Adobe/SDKs/Acrobat 10 SDK/Version 1/PluginSupport/Headers/API")
set(PDFL_LIBRARIES AdobePDFL.lib)

# SPDF
#set(PDFL_INCLUDE_DIRS /usr/apago/src/misc/SPDFsrc/Headers)
#set(PDFL_LIBRARIES /usr/apago/bbuild/apago/misc/SPDFsrc/darwin-4.2.1/release/address-model-32/architecture-x86/link-static/threading-multi/libSPDF_src.a /usr/apago/bbuild/apago/misc/md5/darwin-4.2.1/release/address-model-32/architecture-x86/link-static/threading-multi/libmd5.a /usr/apago/bbuild/apago/libs/apago_utils/darwin-4.2.1/release/address-model-32/architecture-x86/link-static/threading-multi/libapago_utils.a)

include_directories(${INCLUDE_DIRECTORIES} ${PDFL_INCLUDE_DIRS})

target_link_libraries(wrappertest ${PDFL_LIBRARIES} ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES} ${Boost_LIBRARIES})

