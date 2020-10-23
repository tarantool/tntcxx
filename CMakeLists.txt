PROJECT(tntcxx)

CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug)
ENDIF()

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_C_STANDARD 17)
ADD_COMPILE_OPTIONS(-Wall -Wextra -Werror)

INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(./src)
ADD_EXECUTABLE(MempoolUnitTest.test src/Utils/Mempool.hpp test/MempoolUnitTest.cpp)
ADD_EXECUTABLE(CStrUnit.test src/Utils/CStr.hpp test/CStrUnitTest.cpp)
ADD_EXECUTABLE(BufferUnit.test src/Buffer/Buffer.hpp test/BufferUnitTest.cpp)
ADD_EXECUTABLE(EncDecUnit.test src/mpp/mpp.hpp test/EncDecTest.cpp)
add_executable(Client.test src/Client/Connector.hpp test/ClientTest.cpp)

ENABLE_TESTING()
ADD_TEST(NAME MempoolUnitTest.test COMMAND MempoolUnitTest.test)
ADD_TEST(NAME CStrUnit.test COMMAND CStrUnit.test)
ADD_TEST(NAME BufferUnit.test COMMAND BufferUnit.test)
ADD_TEST(NAME EncDecUnit.test COMMAND EncDecUnit.test)
ADD_TEST(NAME Client.test COMMAND Client.test)
