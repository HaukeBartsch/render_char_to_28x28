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
set(boost_min_ver 1.50.0)
set(boost_libs system filesystem timer date_time)
find_package(Boost ${boost_min_ver})

if(Boost_FOUND)
    find_package(Boost ${boost_min_ver} COMPONENTS ${boost_libs})
endif()

add_executable (renderText main.cpp ${CMAKE_CURRENT_SOURCE_DIR})
#target_include_directories (ParseFast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/gdcm-build/Source/DataStructureAndEncodingDefinition ${CMAKE_CURRENT_SOURCE_DIR}/gdcm-build/Source/Common ${CMAKE_CURRENT_SOURCE_DIR}/GDCM-3.0.8/Source/Common ${CMAKE_CURRENT_SOURCE_DIR}/GDCM-3.0.8/Source/MediaStorageAndFileFormat/ ${CMAKE_CURRENT_SOURCE_DIR}/GDCM-3.0.8/Source/InformationObjectDefinition/ ${CMAKE_CURRENT_SOURCE_DIR}/GDCM-3.0.8/Source/DataDictionary/ ${CMAKE_CURRENT_SOURCE_DIR}/GDCM-3.0.8/bin/Source/Common ${CMAKE_CURRENT_SOURCE_DIR}/GDCM-3.0.8/Source/DataStructureAndEncodingDefinition ${LIBXML2_INCLUDE_DIR})
target_include_directories (renderText PUBLIC ${GDCM_BINARY_DIR}/Source/Common ${GDCM_SOURCE_DIR}/Source/Common ${GDCM_SOURCE_DIR}/Source/MediaStorageAndFileFormat ${LIBXML2_INCLUDE_DIR})


#SET(GDCM_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/gdcm-build/bin)
#link_directories(ParseFast ${CMAKE_CURRENT_SOURCE_DIR}/gdcm-build/bin)
#FIND_LIBRARY(COMMON_LIBRARY     NAMES gdcmCommon     PATHS ${GDCM_LIB_PATH} NO_DEFAULT_PATH)
#FIND_LIBRARY(IOD_LIBRARY        NAMES gdcmIOD        PATHS ${GDCM_LIB_PATH} NO_DEFAULT_PATH)
#FIND_LIBRARY(MSFF_LIBRARY       NAMES gdcmMSFF       PATHS ${GDCM_LIB_PATH} NO_DEFAULT_PATH)
#FIND_LIBRARY(DSED_LIBRARY       NAMES gdcmDSED       PATHS ${GDCM_LIB_PATH} NO_DEFAULT_PATH)
#FIND_LIBRARY(DICT_LIBRARY       NAMES gdcmDICT       PATHS ${GDCM_LIB_PATH} NO_DEFAULT_PATH)
#FIND_LIBRARY(MEXD_LIBRARY       NAMES gdcmMEXD       PATHS ${GDCM_LIB_PATH} NO_DEFAULT_PATH)


#message(AUTHOR_WARNING ${COMMON_LIBRARY})
#message(AUTHOR_WARNING ${IOD_LIBRARY})
#message(STATUS COMMON_LIBRARY = ${COMMON_LIBRARY})
#message(STATUS IOD_LIBRARY = ${IOD_LIBRARY})
#message(STATUS DICT_LIBRARY = ${DICT_LIBRARY})
#message(STATUS DSED_LIBRARY = ${DSED_LIBRARY})
#message(STATUS MSFF_LIBRARY = ${MSFF_LIBRARY})
#message(STATUS MEXD_LIBRARY = ${MEXD_LIBRARY})

target_link_libraries(renderText gdcmCommon gdcmMEXD gdcmMEXD gdcmMSFF gdcmDICT gdcmDSED ${LIBXML2_LIBRARY} ${JPEG_LIBRARY} ${ZLIB_LIBRARY} ${XLST_LIBRARY} ${Boost_LIBRARIES} pthread)
