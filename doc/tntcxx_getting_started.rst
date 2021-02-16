
Connecting to Tarantool from C++
=================================

.. //TDB Overview intro


.. //TBD intro about using examples - to place it here or in pre-req?

?
Connecting to Tarantool from your C++ application and sending requests to and
receiving replies from Tarantool while working with data in a database requires
writing code.

To simplify the start of your working with C++ Tarantool connector, we will
use examples from the `connector repository <https://github.com/tarantool/tntcxx/tree/master/examples>`_.
We will go step by step through the code of the example application and explain
what each part does.

.. //https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp


.. _gs_cxx_prereq:

Pre-requisites
----------------

.. //TBD intro paragraph and refs to sub-topics

To go through this Getting started exercise, we need the following
pre-requisites:

* install and build the connector
* start Tarantool and create a database
* set up access rights

.. _gs_cxx_prereq_install:

Installation
~~~~~~~~~~~~~

Currently supported OS is Linux. To install the Tarantool C++ connector in your
OS you should build it from source.

.. //TBD links to install pages

#. Make sure you have the necessary third-party software. If you miss something, install it:

   * Install git, a version control system.
   * Install the unzip utility.
   * Install the gcc compiler complied with C++17 standard.
   * Install the cmake and make tools.

#. Install Tarantool ?1.10 or higher.

   You can:

   Install it from a package (see https://www.tarantool.io/en/download/ for OS-specific instructions).
   Build it from sources (see https://www.tarantool.io/en/download/os-installation/building-from-source/).

#. Clone the Tarantool C++ connector repository and build the connector from source.

   .. code-block:: bash

      git clone git@github.com:tarantool/tntcxx.git
      cd tntcxx
      cmake .
      make

.. _gs_cxx_prereq_tnt_run:

Starting Tarantool and creating a database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:ref:`Start <getting_started_db>` Tarantool (locally or in Docker)
and create a space with the following schema and index:

   .. code-block:: lua

       box.cfg{listen = 3301}
       t = box.schema.space.create('t')
       t:format({
                {name = 'id', type = 'unsigned'},
                {name = 'a', type = 'string'},
                {name = 'b', type = 'number'}
                })
       t:create_index('primary', {
                type = 'hash',
                parts = {'id'}
                })

   .. IMPORTANT::

      Please do not close the terminal window where Tarantool is running.
      You'll need it later to connect to Tarantool from our C++ application.

.. _gs_cxx_prereq_access:

Setting up access rights
~~~~~~~~~~~~~~~~~~~~~~~~

To be able to execute the necessary operations in Tarantool, we need to grant
the ``guest`` user with the read-write rights. The simplest way is to grant
the user with the 'super' :ref:`role <authentication-roles>`:

.. code-block:: lua

   box.schema.user.grant('guest', 'super')

.. //TBD !!!for code snippets in the topics below -- check which of them can be replaced with literalinclude. That also requires setting labels (start-after and end-before ) in the code files via comments

Connecting to Tarantool
-----------------------

.. //TBD intro paragraph, create refs to sub-topics
To set up connection to a Tarantool instance, we need to specify the following
in our C++ application:

* embed the connector into an application
* instantiate a connector client and a connection object
* define connection parameters and invoke the method to connect
* define error handling behavior.

.. _gs_cxx_embedding:

Embedding connector
~~~~~~~~~~~~~~~~~~~

Connector can be embedded in a C++ application by including the main
header:

.. code-block:: c

   #include "<path_to_cloned_repo>/src/Client/Connector.hpp"
.. //https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L40

.. _gs_cxx_instantiation:

Objects instantiation
~~~~~~~~~~~~~~~~~~~~~

To create a client, one should specify buffer's and network provider's
implementations as template parameters. Connector main class has the
following signature:

.. code-block:: c

   template<class BUFFER, class NetProvider = DefaultNetProvider<BUFFER>>
   class Connector;

If your don't want to bother with implementing your own buffer or network
provider, you can use the default ones: ``tnt::Buffer<16 * 1024>`` and
``DefaultNetProvider<tnt::Buffer<16 * 1024>>``.

The default instantiation looks as follows:

.. code-block:: c

   using Buf_t = tnt::Buffer<16 * 1024>;
   using Net_t = DefaultNetProvider<Buf_t >;
   Connector<Buf_t, Net_t> client;

.. // https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L49-L50
.. //https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L104
.. //TBD also write about #include "../src/Buffer/Buffer.hpp"

A client itself is not enough to work with Tarantool instances, so let's
also create connection objects. Connection takes the buffer and the network
provider as template parameters as well (note that they must be the same
as ones of the client):

.. //https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L108

.. code-block:: c

   Connection<Buf_t, Net_t> conn(client);

Connecting
~~~~~~~~~~

Now, assume Tarantool instance is listening the ``3301`` port on ``localhost``.
Let's define the necessary variables as well as the ``WAIT_TIMEOUT`` variable
for connection timeout.

.. // https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L45-L47

.. code-block:: c

   const char *address = "127.0.0.1";
   int port = 3301;
   int WAIT_TIMEOUT = 1000; //milliseconds

To connect to the Tarantool instance, we should invoke
the ``Connector::connect()`` method of client object and
pass three arguments: connection instance, address, and port.

.. code-block:: c

   int rc = client.connect(conn, address, port)

Error handling
~~~~~~~~~~~~~~

Implementation of connector is exception free, so we rely on the return
codes: in case of fail, ``connect()`` returns ``rc < 0``. To get the
error message corresponding to the last error happened during
communication with the instance, we can invoke the ``Connection::getError()``
method:

.. code-block:: c

   if (rc != 0) {
       assert(conn.status.is_failed);
       std::cerr << conn.getError() << std::endl;
   }

To reset connection after errors, that is, to clean up error message and connection
status, the ``Connection::reset()`` method is used.

.. // TBD For more information on connectors API, refer ... <link to the API document>

.. // TBD section title - Manipulating the data vs Requests vs something else

.. _gs_cxx_data_manipulate:

Manipulating the data / Requests
----------------------------------

.. //TBD !!!intro, list of request types, list of logical steps we are going to do, ?other concept points
In this section, you will learn how to:

* prepare different types of requests
* send the requests
* receive and handle responses.

In our example C++ application, we are going to prepare and execute the following
types of requests:

* ping
* replace
* select.

.. NOTE::

   Examples on other request types, namely, ``insert``, ``delete``, ``upsert``,
   and ``update``, will be added in this manual later.

Each request method returns a request ID, which is a sort of `future <https://en.wikipedia.org/wiki/Futures_and_promises>`_.
It can be used to get the result of request execution once it is ready, that is,
response. Requests are queued in the output buffer of connection
until ``Connector::wait()`` is called.

Preparing requests
~~~~~~~~~~~~~~~~~~~

.. //TBD intro

At this step, requests are encoded (into msgpack format) and saved into the
output connection buffer -- they are ready to be sent.
But network communication itself will be done later.

.. //TDB not sure yet if it's better to have a sub-topic for each request type

.. code-block:: c

   /* PING */
   rid_t ping = conn.ping();

   /* REPLACE: equals to space:replace(pk_value, "111", 1)*/
   uint32_t space_id = 512;
   int pk_value = 666;
   std::tuple data = std::make_tuple(pk_value /* field 1*/, "111" /* field 2*/, 1.01 /* field 3*/);
   rid_t replace = conn.space[space_id].replace(data);

   /* SELECT: equals to space.index[0]:select({pk_value}, {limit = 1})*/
   uint32_t index_id = 0;
   uint32_t limit = 1;
   uint32_t offset = 0;
   IteratorType iter = IteratorType::EQ;
   auto i = conn.space[space_id].index[index_id];
   rid_t select = i.select(std::make_tuple(pk_value), limit, offset, iter);

Sending requests
~~~~~~~~~~~~~~~~~

To send requests to the server side, we should invoke the ``client.wait()``
method:

.. code-block:: c

   client.wait(conn, ping, WAIT_TIMEOUT);

The ``wait()`` method takes connection to poll (both IN and OUT),
request ID, and, optionally, timeout parameters. Once
response for specified request is ready, ``wait()`` terminates. It also
provides negative return code in case of system related fails, e.g.
broken or time outed connection. If ``wait()`` returns ``0``, then a response
has been received and expected to be parsed.

Now let's send our requests to the Tarantool instance.
There are two options for single connection: we can either wait for one specific
future or for all of them at once. Let's try both options.

.. code-block:: c

   while (! conn.futureIsReady(ping)) {
      /*
       * wait() is the main function responsible for sending/receiving
       * requests and implements event-loop under the hood. It may
       * fail due to several reasons:
       *  - connection is timed out;
       *  - connection is broken (e.g. closed);
       *  - epoll is failed.
       */
      if (client.wait(conn, ping, WAIT_TIMEOUT) != 0) {
         assert(conn.status.is_failed);
         std::cerr << conn.getError() << std::endl;
         conn.reset();
      }
   }

Receiving responses
~~~~~~~~~~~~~~~~~~~~

To get the response when it is ready, use the
``Connection::getResponse()`` method. It takes the request ID and returns an
optional object containing response (returns ``nullptr`` in case the response
is not ready yet). Note that on each future it can be called only once:
``getResponse()`` erases the request ID from internal map once it is
returned to user.

.. code-block:: c

   std::optional<Response<Buf_t>> response = conn.getResponse(ping);

.. //TBD below is the explanation paragraph -- possibly, to move it to another place

Response consists of a header and a body (``response.header`` and
``response.body``). Depending on success of request execution on the server
side, body may contain either runtime error(s) (accessible by
``response.body.error_stack``) or data (tuples)
(``response.body.data``). In turn, data is a vector of tuples. However,
tuples are not decoded and come in the form of pointers to the start and the end
of msgpacks. See the :ref:`section <gs_cxx_data_readers>` below to understand
how to decode tuples.

.. //TBD perhaps to remove comments from the code example because this is already explained. Or - to leave the explanation in the code quote and remove it in the beginning of the section

.. code-block:: c

   /* Now let's get response using our future.*/
   std::optional<Response<Buf_t>> response = conn.getResponse(ping);

   /*
    * Since conn.futureIsReady(ping) returned <true>, then response
    * must be ready.
    */
   assert(response != std::nullopt);

   /*
    * If request is successfully executed on server side, response
    * will contain data (i.e. tuple being replaced in case of :replace()
    * request or tuples satisfying search conditions in case of :select();
    * responses for pings contain nothing - empty map).
    * To tell responses containing data from error responses, one can
    * rely on response code storing in the header or check
    * Response->body.data and Response->body.error_stack members.
    */
   printResponse<Buf_t>(conn, *response);

.. //TBD some intro about receiving responses for replace and select

.. code-block:: c

   /* Let's wait for both futures at once. */
   rid_t futures[2];
   futures[0] = replace;
   futures[1] = select;

   /* No specified timeout means that we poll futures until they are ready.*/
   client.waitAll(conn, (rid_t *) &futures, 2);
   for (int i = 0; i < 2; ++i) {
      assert(conn.futureIsReady(futures[i]));
      response = conn.getResponse(futures[i]);
      assert(response != std::nullopt);
      printResponse<Buf_t>(conn, *response);
   }

.. //TBD ? Maybe to give the print results for the corresponding code lines

Several connections at once
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let's have a look at the case when we establish two connections to Tarantool
instance simultaneously.

.. code-block:: c

   /* Now create another connection. */
   Connection<Buf_t, Net_t> another(client);
   if (client.connect(another, address, port) != 0) {
      assert(conn.status.is_failed);
      std::cerr << conn.getError() << std::endl;
      return -1;
   }

   /* Simultaneously execute two requests from different connections. */
   rid_t f1 = conn.ping();
   rid_t f2 = another.ping();

   /*
    * waitAny() returns the first connection received response.
    * All connections registered via :connect() call are participating.
    */
   Connection<Buf_t, Net_t> *first = client.waitAny(WAIT_TIMEOUT);
   if (first == &conn) {
      assert(conn.futureIsReady(f1));
   } else {
      assert(another.futureIsReady(f2));
   }

Closing connections
~~~~~~~~~~~~~~~~~~~~

Finally, a user is responsible for closing connections.

.. code-block:: c

   client.close(conn);
   client.close(another);

Building and launching the C++ application
-------------------------------------------

Finally, we are going to build our example C++ application, launch it
to connect to Tarantool instance and execute all the requests defined.

Make sure you are in the root directory of the cloned repository. To build
the example application:

.. code-block:: bash

   cd examples
   cmake .
   make

Make sure the :ref:`Tarantool session <gs_cxx_prereq_tnt_run>`
you started earlier is running. Launch the application:

.. code-block:: bash

   ./Simple

.. //TBD To give parts of the print results with comments

.. _gs_cxx_data_readers:

Decoding and reading the data
------------------------------

Responses from a Tarantool instance contain raw data, that is, encoded into msgpack
tuples. To decode client's data, a user has to write one's own decoders
(based on ?featured schema).

.. //TBD intro paragraphs below -- to edit

To show the logic of decoding a response, we will use
`the reader from our example <https://github.com/tarantool/tntcxx/blob/master/examples/Reader.hpp>`_.
The reader should be included in your application:

.. code-block:: c

   #include "Reader.hpp"

In the reader, first, the structure describing the data stored in space ``t`` should be defined:

.. //TBD replace SQL notation schema description with the Lua one

.. code-block:: c

   /**
    * Corresponds to tuples stored in user's space:
    * box.execute("CREATE TABLE t (id UNSIGNED PRIMARY KEY, a TEXT, d DOUBLE);")
    */
   struct UserTuple {
      uint64_t field1;
      std::string field2;
      double field3;
   };


Base reader prototype
~~~~~~~~~~~~~~~~~~~~~~

Prototype of the base reader is given in ``src/mpp/Dec.hpp``:

.. code-block:: c

   template <class BUFFER, Type TYPE>
   struct SimpleReaderBase : DefaultErrorHandler {
       using BufferIterator_t = typename BUFFER::iterator;
       /* Allowed type of values to be parsed. */
       static constexpr Type VALID_TYPES = TYPE;
       BufferIterator_t* StoreEndIterator() { return nullptr; }
   };

Every new reader should inherit from it or directly from
``DefaultErrorHandler``.

Parsing values
~~~~~~~~~~~~~~~

To parse particular value, we should define the ``Value()`` method.
First two arguments of the method are common and unused as a rule,
but the third one defines the parsed value. In case of POD structures, it's
enough to provide byte-to-byte copy. Since in our schema there are
fields of three different types, let's describe three ``Value()``
functions:

.. code-block:: c

   struct UserTupleValueReader : mpp::DefaultErrorHandler {
       /* Store instance of tuple to be parsed. */
       UserTuple& tuple;

       /* Enumerate all types which can be parsed. Otherwise */
       static constexpr mpp::Type VALID_TYPES = mpp::MP_UINT | mpp::MP_STR | mpp::MP_DBL;
       UserTupleValueReader(UserTuple& t) : tuple(t) {}

       /* Value's extractors. */
       void Value(const BufIter_t&, mpp::compact::Type, uint64_t u)
       {
          tuple.field1 = u;
       }
       void Value(const BufIter_t&, mpp::compact::Type, double d)
       {
           tuple.field3 = d;
       }
       void Value(const BufIter_t& itr, mpp::compact::Type, mpp::StrValue v)
       {
           BufIter_t tmp = itr;
           tmp += v.offset;
           std::string &dst = tuple.field2;
           while (v.size) {
               dst.push_back(*tmp);
               ++tmp;
               --v.size;
           }
       }
   };

Parsing array
~~~~~~~~~~~~~~~

.. //TBD if this should come first, before the parsing values topic?

It is worth mentioning that tuple itself is wrapped into an array, so in
fact firstly we should parse the array. Let's define another reader for that
purpose:

.. code-block:: c

   template <class BUFFER>
   struct UserTupleReader : mpp::SimpleReaderBase<BUFFER, mpp::MP_ARR> {
       mpp::Dec<BUFFER>& dec;
       UserTuple& tuple;

       UserTupleReader(mpp::Dec<BUFFER>& d, UserTuple& t) : dec(d), tuple(t) {}
       void Value(const iterator_t<BUFFER>&, mpp::compact::Type, mpp::ArrValue)
       {
           dec.SetReader(false, UserTupleValueReader{tuple});
       }
   };

Setting reader
~~~~~~~~~~~~~~~

``SetReader();`` sets the reader which is invoked while every entry of
the array is parsed. Now, to make these two readers work, we should
create a decoder, set its iterator to the position of encoded tuple, and
invoke the ``Read()`` method:

.. code-block:: c

   UserTuple tuple;
   mpp::Dec dec(conn.getInBuf());
   dec.SetPosition(*t.begin);
   dec.SetReader(false, UserTupleReader<BUFFER>{dec, tuple});
   dec.Read();


.. // TBD ?other important topics to put in this GS?
