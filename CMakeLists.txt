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

# add PDF library requirements here
set(PDFLIB_INCLUDE_DIRS <path>)
set(PDFLIB_LIBRARIES <path>)
include_directories(${INCLUDE_DIRECTORIES} ${PDFLIB_INCLUDE_DIRS})

target_link_libraries(wrappertest ${PDFLIB_LIBRARIES} ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES} ${Boost_LIBRARIES})
