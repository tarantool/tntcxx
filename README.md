# tntcxx — Tarantool C++ Connector

This repository contains the tntcxx Tarantool C++ connector code. tntcxx is an
open-source Tarantool C++ connector (compliant to C++17) designed with high
efficiency in mind.

## Building tntcxx

[CMake](https://cmake.org/) is the official build system for tntcxx.

### CMake Build Instructions

tntcxx comes with a CMake build script ([CMakeLists.txt](./CMakeLists.txt))
that can be used on a wide range of platforms ("C" stands for cross-platform.).
If you don't have CMake installed already, you can download it for free from
<https://www.cmake.org/>.
CMake works by generating native makefiles or build projects that can
be used in the compiler environment of your choice.
For API/ABI compatibility reasons, we strongly recommend building tntcxx in a
subdirectory of your project or as an embedded dependency.

#### Incorporating tntcxx Into a Cmake Project

##### Step-by-Step Instructions

1. Make tntcxx's source code available to the main build. This can be done a few
different ways:
    * Download the tntcxx source code manually and place it at a known location.
    This is the least flexible approach and can make it more difficult to use
    with continuous integration systems, etc.
    * Embed the tntcxx source code as a direct copy in the main project's source
    tree. This is often the simplest approach, but is also the hardest to keep
    up to date. Some organizations may not permit this method.
    * Add tntcxx as a [git submodule](https://git-scm.com/docs/git-submodule) or
    equivalent. This may not always be possible or appropriate. Git submodules,
    for example, have their own set of advantages and drawbacks.
    * Use the CMake [`FetchContent`](https://cmake.org/cmake/help/latest/module/FetchContent.html)
    commands to download tntcxx as part of the build's configure step. This
    approach doesn't have the limitations of the other methods.

The last of the above methods is implemented with a small piece of CMake code
that downloads and pulls the tntcxx code into the main build. Just add the
following snippet to your CMakeLists.txt:
```cmake
include(FetchContent)
FetchContent_Declare(
  tntcxx
  GIT_REPOSITORY https://github.com/tarantool/tntcxx.git
)
FetchContent_MakeAvailable(tntcxx)
```

After obtaining tntcxx sources using the rest of the methods, you can use the
following CMake command to incorporate tntcxx into your CMake project:
```cmake
add_subdirectory(${TNTCXX_SOURCE_DIR})
```

2. Now simply link against the tntcxx::tntcxx target as needed:
```cmake
add_executable(example example.cpp)
target_link_libraries(example tntcxx::tntcxx)
```

##### Running tntcxx Tests with CMake

Use the `-DTNTCXX_BUILD_TESTING=ON` option to run the tntcxx tests. This option
is enabled by default if the tntcxx project is determined to be the top level
project. Note that `BUILD_TESTING` must also be on (the default).

For example, to run the tntcxx tests, you could use this script:
```console
cd path/to/tntcxx
mkdir build
cd build
cmake -DTNTCXX_BUILD_TESTING=ON ..
make -j
ctest
```

### CMake Option Synopsis

- `-DTNTCXX_BUILD_TESTING=ON` must be set to enable testing. This option is
enabled by default if the tntcxx project is determined to be the top level
project.

## Internals

There are three main parts of C++ connector: IO-zero-copy buffer, msgpack
encoder/decoder and client handling requests itself.

### Buffer

Buffer is parameterized by allocator, which means that users are able to choose
which allocator will be used to provide memory for buffer's blocks.
Data is orginized into linked list of blocks of fixed size which is specified
as template parameter of buffer.

### Client API

**TODO: see src/Client/Connection.hpp and src/Client/Connector.hpp**

## Usage

### Embedding

Connector can be embedded in any C++ application with including main header:
`#include "<path-to-cloned-repo>/src/Client/Connector.hpp"`

### Objects Instantiation

To create client one should specify buffer's and network provider's implementations
as template parameters. Connector's main class has the following signature:

```c++
template<class BUFFER, class NetProvider = EpollNetProvider<BUFFER>>
class Connector;
```

If one don't want to bother with implementing its own buffer or network provider,
one can use default one: `tnt::Buffer<16 * 1024>` and
`EpollNetProvider<tnt::Buffer<16 * 1024>>`.
So the default instantiation would look
like:
```c++
using Buf_t = tnt::Buffer<16 * 1024>;
using Net_t = EpollNetProvider<Buf_t >;
Connector<Buf_t, Net_t> client;
```

Client itself is not enough to work with Tarantool instances, so let's also create
connection objects. Connection takes buffer and network provider as template
parameters as well (note that they must be the same as ones of client):
```c++
Connection<Buf_t, Net_t> conn(client);
```

### Connecting

Now assume Tarantool instance is listening `3301` port on localhost. To connect
to the server we should invoke `Connector::connect()` method of client object and
pass three arguments: connection instance, address and port.
```c++
int rc = client.connect(conn, address, port);
```

### Error Handling

Implementation of connector is exception
free, so we rely on return codes: in case of fail, `connect()` will return `rc < 0`.
To get error message corresponding to the last error happened during communication
with server, we can invoke `Connection::getError()` method:
```c++
if (rc != 0) {
    std::cerr << conn.getError() << std::endl;
}
```

To reset connection after errors (clean up error message and connection status),
one can use `Connection::reset()`.

### Preparing Requests

To execute simplest request (i.e. ping), one can invoke corresponding method of
connection object:
```c++
rid_t ping = conn.ping();
```
Each request method returns request id, which is sort of future. It can be used
to get the result of request execution once it is ready (i.e. response). Requests
are queued in the input buffer of connection until `Connector::wait()` is called.

### Sending Requests

That said, to send requests to the server side, we should invoke `client.wait()`:
```c++
client.wait(conn, ping, WAIT_TIMEOUT);
```
Basically, `wait()` takes connection to poll (both IN and OUT), request id and
optionally timeout (in milliseconds) parameters. once response for specified
request is ready, `wait()` terminates. It also provides negative return code in
case of system related fails (e.g. broken or time outed connection). If `wait()`
returns 0, then response is received and expected to be parsed.

### Receiving Responses

To get the response when it is ready, we can use `Connection::getResponse()`.
It takes request id and returns optional object containing response (`nullptr`
in case response is not ready yet). Note that on each future it can be called
only once: `getResponse()` erases request id from internal map once it is
returned to user.

```c++
std::optional<Response<Buf_t>> response = conn.getResponse(ping);
```
Response consists of header and body (`response.header` and `response.body`).
Depending on success of request execution on server side, body may contain
either runtime error(s) (accessible by `response.body.error_stack`) or data
(tuples) (`response.body.data`). In turn, data is a vector of tuples. However,
tuples are not decoded and come in form of pointers to the start and end of
msgpacks. See section below to understand how to decode tuples.

### Data Manipulation

Now let's consider a bit more sophisticated requests.
Assume we have space with `id = 512` and following format on the server:
`CREATE TABLE t(id INT PRIMARY KEY, a TEXT, b DOUBLE);`
Preparing analogue of `t:replace(1, "111", 1.01);` request can be done this way:

```c++
std::tuple data = std::make_tuple(1 /* field 1*/, "111" /* field 2*/, 1.01 /* field 3*/);
rid_t replace = conn.space[512].replace(data);
```
To execute select query `t.index[1]:select({1}, {limit = 1})`:

```c++
auto i = conn.space[512].index[1];
rid_t select = i.select(std::make_tuple(1), 1, 0 /*offset*/, IteratorType::EQ);
```

### Data Readers

Responses from server contain raw data (i.e. encoded into MsgPack tuples).
Let's define structure describing data stored in space `t`:

```c++
struct UserTuple {
    uint64_t field1;
    std::string field2;
    double field3;

    static constexpr auto mpp = std::make_tuple(
        &UserTuple::field1, &UserTuple::field2, &UserTuple::field3);
};
```

Member `mpp` is neccessary - it sets the relationship between the structure
members and associated tuple's fields. It is used by encoder and decoder
for Object <-> MsgPack serialization. For instance, such structure will be
serialied as a MsgPack array `[<field1>, <field2>, <field3>]`. If you need
to serialize non-static members of objects,
[pointers to data members](https://en.cppreference.com/w/cpp/language/pointer#Pointers_to_data_members)
can be used, just as in this example.

Let's get back to the example with `select`. Consider the request successful.
We can decode data in this way:

```c++
if (response.body.data != std::nullopt) {
    std::vector<UserTuple> results;
    bool ok = response.body.data->decode(results);
    if (ok)
        print_results(results);
}
```

Firstly, we check if the response actually contains any data (Tarantool has
sent `IPROTO_DATA` in response). According to
[`IPROTO` protocol](https://www.tarantool.io/ru/doc/latest/reference/internals/box_protocol/),
key `IPROTO_DATA`
[has](https://www.tarantool.io/ru/doc/latest/reference/internals/iproto/format/#body)
an array of tuples as value in response to `select`. So, in order to
successfully decode them, we should pass an array of tuples to decoder - that's
why `std::vector<UserTuple>` is needed. If decoding was successful, `results`
will contain all decoded `UserTuples`.

## Multi-Threading Model

A `Connector` object and all its instances of `Connection` objects must be used in a single thread. For multi-threaded usage, create one or several `Connector` instances for each thread. Each `Connection` object must be used only with the `Connector` object that it was created from.

If custom `Buffer` or `NetProvider` implementations are used for `Connector` objects, the custom implementations must not share any state (e.g., `static` fields).
