CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

PROJECT(tntcxx)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug)
ENDIF()

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
FIND_PACKAGE (benchmark QUIET)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_C_STANDARD 11)
ADD_COMPILE_OPTIONS(-Wall -Wextra -Werror)

ADD_LIBRARY(tntcxx INTERFACE)
ADD_LIBRARY(tntcxx::tntcxx ALIAS tntcxx)
TARGET_INCLUDE_DIRECTORIES(tntcxx INTERFACE .)
TARGET_INCLUDE_DIRECTORIES(tntcxx INTERFACE ./src)

ADD_LIBRARY(ev STATIC third_party/libev/ev.c)
TARGET_COMPILE_DEFINITIONS(ev PRIVATE EV_STANDALONE=1)
TARGET_COMPILE_OPTIONS(ev PRIVATE -w)
TARGET_INCLUDE_DIRECTORIES(ev PUBLIC ./third_party/libev)

TARGET_LINK_LIBRARIES(tntcxx INTERFACE ev)

SET(COMMON_LIB tntcxx ev)

# OpenSSL
IF (TNTCXX_ENABLE_SSL)
FIND_PACKAGE(OpenSSL)
IF (OPENSSL_FOUND)
    MESSAGE(STATUS "OpenSSL ${OPENSSL_VERSION} found")
    TARGET_INCLUDE_DIRECTORIES(tntcxx INTERFACE ${OPENSSL_INCLUDE_DIR})
ELSE()
    MESSAGE(FATAL_ERROR "Could NOT find OpenSSL development files (libssl-dev/openssl-devel package)")
ENDIF()

# OpenSSL can require Z library (depending on build time options), so we add
# it to libraries list in case of static openssl linking.
IF(OPENSSL_USE_STATIC_LIBS)
    SET(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES})
ENDIF()

SET(COMMON_LIB ${COMMON_LIB} ${OPENSSL_LIBRARIES})

ENDIF() # IF (TNTCXX_ENABLE_SSL)

# Compensating the lack of PROJECT_IS_TOP_LEVEL for older cmake version.
IF (CMAKE_VERSION VERSION_LESS 3.21)
    # Strictly speaking it is not equivalent but suitable for us.
    IF (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        SET(PROJECT_IS_TOP_LEVEL ON)
    ENDIF()
ENDIF()

SET(TNTCXX_BUILD_TESTING_DEFAULT OFF)

IF (PROJECT_IS_TOP_LEVEL)
    SET(TNTCXX_BUILD_TESTING_DEFAULT ON)
ENDIF()

OPTION(TNTCXX_BUILD_TESTING
    "If ON, tntcxx will build all of tntcxx's own tests."
    ${TNTCXX_BUILD_TESTING_DEFAULT})

IF (TNTCXX_BUILD_TESTING)
    INCLUDE(CTest)
    CONFIGURE_FILE(./test/cfg.lua test_cfg.lua COPYONLY)
    CONFIGURE_FILE(./test/cfg_ssl.lua test_cfg_ssl.lua COPYONLY)
    CONFIGURE_FILE(./test/gen_ssl.sh test_gen_ssl.sh COPYONLY)
ENDIF()

# Common function for building tests.
# Arguments are NAME, TYPE, SOURCES, LIBRARIES, DEFINES.
# TYPE should be one of:
#  "ctest" (the test is added to ctest list with ADD_TEST call)
#  "perftest" (nothing special but for future purposes)
#  "gperftest" (built only if benchmark_FOUND, linked to benchmark::benchmark)
#  "other"
FUNCTION (TNTCXX_TEST)
    CMAKE_PARSE_ARGUMENTS(TNTCXX_TEST
            ""
            "NAME;TYPE"
            "SOURCES;LIBRARIES;DEFINES"
            ${ARGN}
    )

    IF (NOT TNTCXX_BUILD_TESTING)
        RETURN()
    ENDIF()
    IF ((${TNTCXX_TEST_TYPE} STREQUAL "gperftest") AND NOT benchmark_FOUND)
        RETURN()
    ENDIF()

    ADD_EXECUTABLE(${TNTCXX_TEST_NAME})
    TARGET_SOURCES(${TNTCXX_TEST_NAME} PRIVATE ${TNTCXX_TEST_SOURCES})
    TARGET_LINK_LIBRARIES(${TNTCXX_TEST_NAME} PRIVATE ${TNTCXX_TEST_LIBRARIES})
    IF (${TNTCXX_TEST_TYPE} STREQUAL "gperftest")
        TARGET_LINK_LIBRARIES(${TNTCXX_TEST_NAME} PRIVATE benchmark::benchmark)
    ENDIF()
    TARGET_COMPILE_DEFINITIONS(${TNTCXX_TEST_NAME} PRIVATE ${TNTCXX_TEST_DEFINES})
    IF (${TNTCXX_TEST_TYPE} STREQUAL "ctest")
        ADD_TEST(NAME ${TNTCXX_TEST_NAME} COMMAND ${TNTCXX_TEST_NAME})
    ENDIF()
ENDFUNCTION()

TNTCXX_TEST(NAME MempoolUnit.test TYPE ctest
            SOURCES src/Utils/Mempool.hpp test/MempoolUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME CStrUnit.test TYPE ctest
            SOURCES src/Utils/CStr.hpp test/CStrUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME CommonUnit.test TYPE ctest
            SOURCES src/Utils/Common.hpp test/CommonUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME TraitsUnit.test TYPE ctest
            SOURCES src/Utils/Traits.hpp test/TraitsUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME RefVectorUnit.test TYPE ctest
            SOURCES src/Utils/RefVector.hpp test/RefVectorUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME ItrRangeUnit.test TYPE ctest
            SOURCES src/Utils/ItrRange.hpp test/ItrRangeUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME Base64Unit.test TYPE ctest
            SOURCES src/Utils/Base64.hpp test/Base64UnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME BufferUnit.test TYPE ctest
            SOURCES src/Buffer/Buffer.hpp test/BufferUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME RingUnit.test TYPE ctest
            SOURCES src/Utils/Ring.hpp test/RingUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME ListUnit.test TYPE ctest
            SOURCES src/Utils/List.hpp test/ListUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME RulesUnit.test TYPE ctest
            SOURCES src/mpp/Rules.hpp test/RulesUnitTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME EncDecUnit.test TYPE ctest
            SOURCES src/mpp/mpp.hpp test/EncDecTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME Client.test TYPE ctest
            SOURCES src/Client/Connector.hpp test/ClientTest.cpp
            LIBRARIES ${COMMON_LIB}
)

IF (TNTCXX_ENABLE_SSL)
    TNTCXX_TEST(NAME ClientSSL.test TYPE ctest
               SOURCES src/Client/Connector.hpp test/ClientTest.cpp
               LIBRARIES ${COMMON_LIB}
               DEFINES TNTCXX_ENABLE_SSL
    )
ENDIF()

TNTCXX_TEST(NAME BufferPerf.test TYPE perftest
           SOURCES src/Buffer/Buffer.hpp test/BufferPerfTest.cpp
)

TNTCXX_TEST(NAME BufferGPerf.test TYPE gperftest
            SOURCES src/Buffer/Buffer.hpp test/BufferGPerfTest.cpp
)

TNTCXX_TEST(NAME ClientPerf.test TYPE perftest
            SOURCES src/Client/Connector.hpp test/ClientPerfTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME EncDecGPerf.test TYPE gperftest
            SOURCES src/mpp/mpp.hpp test/EncDecGPerfTest.cpp test/hints.c
)

TNTCXX_TEST(NAME SimpleExample TYPE other
            SOURCES examples/Simple.cpp
            LIBRARIES ${COMMON_LIB}
)
