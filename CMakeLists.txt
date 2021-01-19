cmake_minimum_required (VERSION 3.16.0)

project (renderText)

# set (CMAKE_CXX_STANDARD 11)
IF("${CMAKE_SYSTEM}" MATCHES "Linux")
   set (CMAKE_CXX_STANDARD 17)
   SET(CMAKE_CXX_FLAGS "-std=c++17 -I/usr/local/include -I/usr/include")
ELSE()
   # find the correct path for isysroot with `xcrun --show-sdk-path`
   # /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
   # SET(CMAKE_CXX_FLAGS "-std=c++11 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -I/usr/local/include")
   SET(CMAKE_CXX_FLAGS "-std=c++11 -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -I/usr/local/include")
ENDIF()

FIND_LIBRARY(LIBXML2_LIBRARY NAMES xml2)
FIND_LIBRARY(JPEG_LIBRARY NAMES jpeg)
FIND_LIBRARY(ZLIB_LIBRARY NAMES z)
FIND_LIBRARY(XLST_LIBRARY NAMES exslt)

message(STATUS LIBXML2_LIBRARY = ${LIBXML2_LIBRARY})
message(STATUS JPEG_LIBRARY = ${JPEG_LIBRARY})
message(STATUS ZLIB_LIBRARY = ${ZLIB_LIBRARY})
message(STATUS XLST_LIBRARY = ${XLST_LIBRARY})

# GDCM
set (GDCM_USE_VTK OFF)
FIND_PACKAGE(GDCM REQUIRED)
    IF(GDCM_FOUND)
        INCLUDE(${GDCM_USE_FILE})

    ELSE(GDCM_FOUND)
        MESSAGE(FATAL_ERROR "Cannot find GDCM, did you set GDCM_DIR?")
    ENDIF(GDCM_FOUND)


# Boost
set(boost_min_ver 1.70.0)
set(boost_libs system filesystem timer date_time)
find_package(Boost ${boost_min_ver})

if(Boost_FOUND)
    find_package(Boost ${boost_min_ver} COMPONENTS ${boost_libs})
endif()

# Freetype
find_package(Freetype REQUIRED)

add_executable (renderText main.cpp ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (renderText PUBLIC ${GDCM_BINARY_DIR}/Source/Common ${GDCM_SOURCE_DIR}/Source/Common ${GDCM_SOURCE_DIR}/Source/MediaStorageAndFileFormat ${LIBXML2_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})

target_link_libraries(renderText gdcmCommon gdcmMEXD gdcmMEXD gdcmMSFF gdcmDICT gdcmDSED ${LIBXML2_LIBRARY} ${JPEG_LIBRARY} ${ZLIB_LIBRARY} ${XLST_LIBRARY} ${Boost_LIBRARIES} ${FREETYPE_LIBRARIES} pthread)
