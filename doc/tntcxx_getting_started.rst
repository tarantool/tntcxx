
.. //TBD Main title

Connecting to Tarantool from C++
=================================

.. //TDB Overview intro


.. //TBD intro about using examples - to place it here or in pre-req.>

?
Connecting to Tarantool from your C++ application and sending requests to and
receiving replies from Tarantool while working with data in a database requires
writing code.

To simplify the start of your working with C++ Tarantool connector, we will be
using examples from the `connector repository <https://github.com/tarantool/tntcxx/tree/master/examples>`_.
We will go step by step through the code of example application and explain
what each part of the code does.

https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp


.. _gs_cxx_prerequisites:

Pre-requisites
----------------

.. //TBD intro paragraph

To start working/To do this getting started exercise with the C++ Tarantool connector we need to prepare the following pre-requisites:

* install and build the connector in the OS
* start Tarantool and create a database with a schema
* ?set up access rights


Installation
~~~~~~~~~~~~

.. //TBD either the static content here or a link to tntcxx reamdme

Starting Tarantool and creating a database
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:ref:`Start <getting_started_db>` Tarantool (locally or in Docker)
and create a database/?a space with the following schema:

   .. code-block:: lua

       box.cfg{listen = 3301}
       t = box.schema.space.create('t')
       t:format({
                {name = 'id', type = 'integer'},
                {name = 'a', type = 'string'},
                {name = 'b', type = 'double'}
                })
       t:create_index('primary', {
                type = 'hash',
                parts = {'id'}
                })


   .. IMPORTANT::

       Please do not close the terminal window
       where Tarantool is running -- you'll need it soon.

.. //TBD to check if we need important note above
.. //TBD to check if we need the step below

[TBD do we need this in our case?] Setting up access rights
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In order to connect to Tarantool as an administrator, reset the password
for the ``admin`` user:

   .. code-block:: lua

       box.schema.user.passwd('pass')

.. //TBD for all code snippets -- choose the way: explicit code-block or literalinclude or/and link to lines in Simple.cpp in repo


Connecting to Tarantool
-----------------------

.. //TBD some intro with the list of todos


.. _gs_cxx_embedding:

Embedding connector
~~~~~~~~~~~~~~~~~~~~~

Connector can be embedded in a C++ application by including main
header: ``#include "<path-to-cloned-repo>/src/Client/Connector.hpp"``

https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L40


.. //TBD restructure to separate pre-requisites and actual object creation

.. _gs_cxx_instantiation:

Objects instantiation
~~~~~~~~~~~~~~~~~~~~~~

To create client one should specify buffer's and network provider's
implementations as template parameters. Connector's main class has the
following signature:

.. code-block:: c

   template<class BUFFER, class NetProvider = DefaultNetProvider<BUFFER>>
   class Connector;

If one don't want to bother with implementing one's own buffer or network
provider, one can use the default ones: ``tnt::Buffer<16 * 1024>`` and
``DefaultNetProvider<tnt::Buffer<16 * 1024>>``.

So the default instantiation would look like:

.. code-block:: c

   using Buf_t = tnt::Buffer<16 * 1024>;
   using Net_t = DefaultNetProvider<Buf_t >;
   Connector<Buf_t, Net_t> client;

https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L49-L50
https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L104

Client itself is not enough to work with Tarantool instances, so let's
also create connection objects. Connection takes buffer and network
provider as template parameters as well (note that they must be the same
as ones of client):

.. //https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L108

.. code-block:: c

   Connection<Buf_t, Net_t> conn(client);


Connecting
~~~~~~~~~~~~

Now assume Tarantool instance is listening ``3301`` port on localhost.

.. // https://github.com/tarantool/tntcxx/blob/master/examples/Simple.cpp#L45-L47

.. code-block:: c

   const char *address = "127.0.0.1";
   int port = 3301;
   int WAIT_TIMEOUT = 1000; //milliseconds


To connect to the server we should invoke ``Connector::connect()``
method of client object and pass three arguments: connection instance,
address and port.
``int rc = client.connect(conn, address, port)``.


Error handling
~~~~~~~~~~~~~~

Implementation of connector is exception free, so we rely on return
codes: in case of fail, ``connect()`` will return ``rc < 0``. To get
error message corresponding to the last error happened during
communication with server, we can invoke ``Connection::getError()``
method:

.. code-block:: c

   if (rc != 0) {
       assert(conn.status.is_failed);
       std::cerr << conn.getError() << std::endl;
   }

To reset connection after errors (clean up error message and connection
status), one can use ``Connection::reset()``.


.. _gs_cxx_data_manipulate:

Manipulating the data / Requests
----------------------------------

.. //TBD intro, list of request types, other concept points

Now, let's execute the following ?types of requests:

* ping
* replace
* select.

Note that any of :request() methods can't fail. They always
return the request ID - the future (number) which is used to get
response once it is received. Also note that at this step,
requests are encoded (into msgpack format) and saved into
output connection's buffer - they are ready to be sent.
But network communication itself will be done later.

Each request method returns request id, which is sort of future. It
can be used to get the result of request execution once it is ready
(i.e. response). Requests are queued in the input buffer of connection
until ``Connector::wait()`` is called.

Preparing requests
~~~~~~~~~~~~~~~~~~~

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

To send requests to the server side, we should invoke ``client.wait()``:

.. code-block:: c

   client.wait(conn, ping, WAIT_TIMEOUT);

Basically, ``wait()`` takes connection to poll (both IN and OUT),
request id and optionally timeout (in milliseconds) parameters. once
response for specified request is ready, ``wait()`` terminates. It also
provides negative return code in case of system related fails (e.g.
broken or time outed connection). If ``wait()`` returns 0, then response
is received and expected to be parsed.

Now let's send our requests to the Tarantool instance [?server].
There are two options for single connection: we can either wait for one specific
future or for all at once. Let's try both options.

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

To get the response when it is ready, we can use
``Connection::getResponse()``. It takes request id and returns optional
object containing response (``nullptr`` in case response is not ready
yet). Note that on each future it can be called only once:
``getResponse()`` erases request id from internal map once it is
returned to user.

.. code-block:: c

   std::optional<Response<Buf_t>> response = conn.getResponse(ping);

.. //TBD below is explanation paragraph -- possible to move to another place

Response consists of header and body (``response.header`` and
``response.body``). Depending on success of request execution on server
side, body may contain either runtime error(s) (accessible by
``response.body.error_stack``) or data (tuples)
(``response.body.data``). In turn, data is a vector of tuples. However,
tuples are not decoded and come in form of pointers to the start and end
of msgpacks. See section below to understand how to decode tuples.

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

Several connections at once
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let's have a look at the case when we establish two connections to Tarantool
instance simultaneously.

.. code-block:: c

   /* Now create another one connection. */
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

.. code-block:: c

   /* Finally, user is responsible for closing connections. */
   client.close(conn);
   client.close(another);


Building and launching the C++ application
-------------------------------------------

.. // TBD using https://github.com/tarantool/tntcxx/blob/master/examples/Makefile

Make sure you are in the root directory of the tntcxx repository.

.. code-block:: bash

   cd examples
   cmake .
   make

.. _gs_cxx_data_readers:

Decoding and reading the data
------------------------------

Responses from server contain raw data (i.e. encoded into msgpuck
tuples). To decode client's data, users have to write their own decoders
(based on featured schema).


Let's define structure describing data stored in space ``t``:

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

Parsing values
~~~~~~~~~~~~~~~

So every new reader should inherit from it or directly from
``DefaultErrorHandler``. To parse particular value, we should define
``Value()`` method. First two arguments are common and unused as a rule,
but the third - defines parsed value. So in case of POD stuctures it's
enough to provide byte-to-byte copy. Since in our schema there are
fields of three different types, let's descripe three ``Value()``
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

.. //TBD if this should come first, before the parsing values?

It is worth mentioning that tuple itself is wrapped into array, so in
fact firstly we should parse array. Let's define another one reader:

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
create decoder, set its iterator to the position of encoded tuple and
invoke ``Read()`` method:

.. code-block:: c

   UserTuple tuple;
   mpp::Dec dec(conn.getInBuf());
   dec.SetPosition(*t.begin);
   dec.SetReader(false, UserTupleReader<BUFFER>{dec, tuple});
   dec.Read();


.. // TBD if there should be other important topic to place in the GS?
