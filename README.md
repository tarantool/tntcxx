# tntcxx

Tarantool C++ connector

## Internals

There are three main parts of C++ connector: IO-zero-copy buffer, msgpack
encoder/decoder and client handling requests itself.

### Buffer

Buffer is parameterized by allocator, which means that users are able to choose
which allocator will be used to provide memory for buffer's blocks.
Data is orginized into linked list of blocks of fixed size which is specified
as template parameter of buffer.

## Build

cmake .
make

