PROJECT(tntcxx)

CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug)
ENDIF()

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
FIND_PACKAGE (benchmark QUIET)

SET(COMMON_LIB ev)

# OpenSSL
IF (TNTCXX_ENABLE_SSL)
FIND_PACKAGE(OpenSSL)
IF (OPENSSL_FOUND)
    MESSAGE(STATUS "OpenSSL ${OPENSSL_VERSION} found")
    INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
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

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_C_STANDARD 11)
ADD_COMPILE_OPTIONS(-Wall -Wextra -Werror)

INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(./src)
INCLUDE_DIRECTORIES(./third_party/libev)
ADD_LIBRARY(ev STATIC third_party/libev/ev.c)
TARGET_COMPILE_DEFINITIONS(ev PRIVATE EV_STANDALONE=1)
TARGET_COMPILE_OPTIONS(ev PRIVATE -w)

# Compensating the lack of PROJECT_IS_TOP_LEVEL for older cmake version.
IF (CMAKE_VERSION VERSION_LESS 3.21)
    # Strictly speaking it is not equivalent but suitable for us.
    IF (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR})
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

FUNCTION (TNTCXX_TEST)
    IF (NOT TNTCXX_BUILD_TESTING)
        RETURN()
    ENDIF()

    CMAKE_PARSE_ARGUMENTS(TNTCXX_TEST
            ""
            "NAME"
            "SOURCES;LIBRARIES;DEFINES"
            ${ARGN}
    )

    SET(_NAME "${TNTCXX_TEST_NAME}.test")

    MESSAGE(STATUS "Creating unit test ${_NAME}")
    ADD_EXECUTABLE(${_NAME})
    TARGET_SOURCES(${_NAME} PRIVATE ${TNTCXX_TEST_SOURCES})
    TARGET_LINK_LIBRARIES(${_NAME} PRIVATE ${TNTCXX_TEST_LIBRARIES})
    TARGET_COMPILE_DEFINITIONS(${_NAME} PRIVATE ${TNTCXX_TEST_DEFINES})
    ADD_TEST(NAME ${_NAME} COMMAND ${_NAME})
ENDFUNCTION()

TNTCXX_TEST(NAME MempoolUnitTest
            SOURCES src/Utils/Mempool.hpp test/MempoolUnitTest.cpp
)

TNTCXX_TEST(NAME CStrUnit
            SOURCES src/Utils/CStr.hpp test/CStrUnitTest.cpp
)

TNTCXX_TEST(NAME CommonUnit
            SOURCES src/Utils/Common.hpp test/CommonUnitTest.cpp
)

TNTCXX_TEST(NAME TraitsUnit
            SOURCES src/Utils/Traits.hpp test/TraitsUnitTest.cpp
)

TNTCXX_TEST(NAME RefVectorUnit
            SOURCES src/Utils/RefVector.hpp test/RefVectorUnitTest.cpp
)

TNTCXX_TEST(NAME ItrRangeUnit
            SOURCES src/Utils/ItrRange.hpp test/ItrRangeUnitTest.cpp
)

TNTCXX_TEST(NAME Base64Unit
            SOURCES src/Utils/Base64.hpp test/Base64UnitTest.cpp
)

TNTCXX_TEST(NAME BufferUnit
            SOURCES src/Buffer/Buffer.hpp test/BufferUnitTest.cpp
)

TNTCXX_TEST(NAME BufferPerf
            SOURCES src/Buffer/Buffer.hpp test/BufferPerfTest.cpp
)

TNTCXX_TEST(NAME RingUnit
            SOURCES src/Utils/Ring.hpp test/RingUnitTest.cpp
)

TNTCXX_TEST(NAME ListUnit
            SOURCES src/Utils/List.hpp test/ListUnitTest.cpp
)

TNTCXX_TEST(NAME RulesUnit
            SOURCES src/mpp/Rules.hpp test/RulesUnitTest.cpp
)

TNTCXX_TEST(NAME EncDecUnit
            SOURCES src/mpp/mpp.hpp test/EncDecTest.cpp
)

TNTCXX_TEST(NAME Client
            SOURCES src/Client/Connector.hpp test/ClientTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME ClientPerfTest
            SOURCES src/Client/Connector.hpp test/ClientPerfTest.cpp
            LIBRARIES ${COMMON_LIB}
)

TNTCXX_TEST(NAME SimpleExample
            SOURCES examples/Simple.cpp
            LIBRARIES ${COMMON_LIB}
)

IF (TNTCXX_ENABLE_SSL)
    TNTCXX_TEST(NAME ClientSSL
                SOURCES src/Client/Connector.hpp test/ClientTest.cpp
                LIBRARIES ${COMMON_LIB}
                DEFINES TNTCXX_ENABLE_SSL
    )
ENDIF()

IF (benchmark_FOUND)
    TNTCXX_TEST(NAME BufferGPerf
                SOURCES src/Buffer/Buffer.hpp test/BufferGPerfTest.cpp
                LIBRARIES benchmark::benchmark
    )
ENDIF()
