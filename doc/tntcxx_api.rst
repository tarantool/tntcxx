.. _tntcxx_api:

Tarantool C++ connector API
===========================

The official C++ connector for Tarantool is located in the
`tanartool/tntcxx <https://github.com/tarantool/tntcxx/>`_ repository.

It is not supplied as part of the Tarantool repository and requires additional
actions for usage.
The connector itself is a header-only library and, as such, doesn't require
installation and building. All you need is to clone the connector
source code and embed it in your C++ project. See the :doc:`C++ connector Getting started </getting_started/getting_started_cxx>`
document for details and examples.

Below is the description of the connector public API.

.. contents::
   :local:
   :depth: 2

.. _tntcxx_api_connector:

Connector class
---------------

..  cpp:class:: template<class BUFFER, class NetProvider = DefaultNetProvider<BUFFER>> \
                class Connector;

    The ``Connector`` class is a template class that defines a connector client
    which can handle many connections to Tarantool instances asynchronously.

    To instantiate a client, you should specify the buffer and the network provider
    implementations as template parameters. You can either implement your own buffer
    or network provider or use the default ones.

    The default connector instantiation looks as follows:

    ..  code-block:: cpp

        using Buf_t = tnt::Buffer<16 * 1024>;
        using Net_t = DefaultNetProvider<Buf_t >;
        Connector<Buf_t, Net_t> client;


Public methods
~~~~~~~~~~~~~~

* :ref:`connect() <tntcxx_api_connector_connect>`
* :ref:`wait() <tntcxx_api_connector_wait>`
* :ref:`waitAll() <tntcxx_api_connector_waitall>`
* :ref:`waitAny() <tntcxx_api_connector_waitany>`
* :ref:`close() <tntcxx_api_connector_close>`

.. _tntcxx_api_connector_connect:

..  cpp:function:: int connect(Connection<BUFFER, NetProvider> &conn, const std::string_view& addr, unsigned port, size_t timeout = DEFAULT_CONNECT_TIMEOUT)

    Connects to a Tarantool instance that is listening on ``addr:port``.
    On successful connection, the method returns ``0``. If the host
    doesn't reply within the timeout period or another error occurs,
    it returns ``-1``. Then, :ref:`Connection.getError() <tntcxx_api_connection_geterror>`
    gives the error message.

    :param conn: object of the :ref:`Connection <tntcxx_api_connection>`
                 class.
    :param addr: address of the host where a Tarantool instance is running.
    :param port: port that a Tarantool instance is listening on.
    :param timeout: connection timeout, seconds. Optional. Defaults to ``2``.

    :return: ``0`` on success, or ``-1`` otherwise.
    :rtype: int

    **Possible errors:**

    *   connection timeout
    *   refused to connect (due to incorrect address or/and port)
    *   system errors: a socket can't be created; failure of any of the system
        calls (``fcntl``, ``select``, ``send``, ``receive``).

    **Example:**

    ..  code-block:: cpp

        using Buf_t = tnt::Buffer<16 * 1024>;
        using Net_t = DefaultNetProvider<Buf_t >;

        Connector<Buf_t, Net_t> client;
        Connection<Buf_t, Net_t> conn(client);

        int rc = client.connect(conn, "127.0.0.1", 3301);

.. _tntcxx_api_connector_wait:

..  cpp:function:: int wait(Connection<BUFFER, NetProvider>& conn, rid_t future, int timeout = 0)

    The main method responsible for sending a request and checking the response
    readiness.

    You should prepare a request beforehand by using the necessary
    method of the :ref:`Connection <tntcxx_api_connection>` class, such as
    :ref:`ping() <tntcxx_api_connection_ping>`
    and so on, which encodes the request
    in the `MessagePack <https://msgpack.org/>`_ format and saves it in
    the output connection buffer.

    ``wait()`` sends the request and is polling the ``future`` for the response
    readiness. Once the response is ready, ``wait()`` returns ``0``.
    If at ``timeout`` the response isn't ready or another error occurs,
    it returns ``-1``. Then, :ref:`Connection.getError() <tntcxx_api_connection_geterror>`
    gives the error message.
    ``timeout = 0`` means the method is polling the ``future`` until the response
    is ready.

    :param conn: object of the :ref:`Connection <tntcxx_api_connection>`
                 class.
    :param future: request ID returned by a request method of
                    the :ref:`Connection <tntcxx_api_connection>` class, such as,
                    :ref:`ping() <tntcxx_api_connection_ping>`
                    and so on.
    :param timeout: waiting timeout, milliseconds. Optional. Defaults to ``0``.

    :return: ``0`` on receiving a response, or ``-1`` otherwise.
    :rtype: int

    **Possible errors:**

    *   timeout exceeded
    *   other possible errors depend on a network provider used.
        If the ``DefaultNetProvider`` is used, failing of the ``poll``,
        ``read``, and ``write`` system calls leads to system errors,
        such as, ``EBADF``, ``ENOTSOCK``, ``EFAULT``, ``EINVAL``, ``EPIPE``,
        and ``ENOTCONN`` (``EWOULDBLOCK`` and ``EAGAIN`` don't occur
        in this case).

    **Example:**

    ..  code-block:: cpp

        client.wait(conn, ping, WAIT_TIMEOUT)

.. _tntcxx_api_connector_waitall:

..  cpp:function:: void waitAll(Connection<BUFFER, NetProvider>& conn, rid_t *futures, size_t future_count, int timeout = 0)

    Similar to :ref:`wait() <tntcxx_api_connector_wait>`, the method sends
    the requests prepared and checks the response readiness, but can send
    several different requests stored in the ``futures`` array.
    Exceeding the timeout leads to an error; :ref:`Connection.getError() <tntcxx_api_connection_geterror>`
    gives the error message.
    ``timeout = 0`` means the method is polling the ``futures``
    until all the responses are ready.

    :param conn: object of the :ref:`Connection <tntcxx_api_connection>`
                 class.
    :param *futures: array with the request IDs returned by request
                     methods of the :ref:`Connection <tntcxx_api_connection>`
                     class, such as, :ref:`ping() <tntcxx_api_connection_ping>`
                     and so on.
    :param future_count: size of the ``futures`` array.
    :param timeout: waiting timeout, milliseconds. Optional. Defaults to ``0``.

    :return: none
    :rtype: none

    **Possible errors:**

    *   timeout exceeded
    *   other possible errors depend on a network provider used.
        If the ``DefaultNetProvider`` is used, failing of the ``poll``,
        ``read``, and ``write`` system calls leads to system errors,
        such as, ``EBADF``, ``ENOTSOCK``, ``EFAULT``, ``EINVAL``, ``EPIPE``,
        and ``ENOTCONN`` (``EWOULDBLOCK`` and ``EAGAIN`` don't occur
        in this case).

    **Example:**

    ..  code-block:: cpp

        rid_t futures[2];
        futures[0] = replace;
        futures[1] = select;

        client.waitAll(conn, (rid_t *) &futures, 2);

.. _tntcxx_api_connector_waitany:

..  cpp:function:: Connection<BUFFER, NetProvider>* waitAny(int timeout = 0)

    Sends all requests that are prepared at the moment and is waiting for
    any first response to be ready. Upon the response readiness, ``waitAny()``
    returns the corresponding connection object.
    If at ``timeout`` no response is ready or another error occurs, it returns
    ``nullptr``. Then, :ref:`Connection.getError() <tntcxx_api_connection_geterror>`
    gives the error message.
    ``timeout = 0`` means no time limitation while waiting for the response
    readiness.

    :param timeout: waiting timeout, milliseconds. Optional. Defaults to ``0``.

    :return: object of the :ref:`Connection <tntcxx_api_connection>` class
             on success, or ``nullptr`` on error.
    :rtype: Connection<BUFFER, NetProvider>*

    **Possible errors:**

    *   timeout exceeded
    *   other possible errors depend on a network provider used.
        If the ``DefaultNetProvider`` is used, failing of the ``poll``,
        ``read``, and ``write`` system calls leads to system errors,
        such as, ``EBADF``, ``ENOTSOCK``, ``EFAULT``, ``EINVAL``, ``EPIPE``,
        and ``ENOTCONN`` (``EWOULDBLOCK`` and ``EAGAIN`` don't occur
        in this case).

    **Example:**

    ..  code-block:: cpp

        rid_t f1 = conn.ping();
        rid_t f2 = another_conn.ping();

        Connection<Buf_t, Net_t> *first = client.waitAny(WAIT_TIMEOUT);
        if (first == &conn) {
            assert(conn.futureIsReady(f1));
        } else {
            assert(another_conn.futureIsReady(f2));
        }

.. _tntcxx_api_connector_close:

..  cpp:function:: void close(Connection<BUFFER, NetProvider> &conn)

    Closes the connection established earlier by
    the :ref:`connect() <tntcxx_api_connector_connect>` method.

    :param conn: connection object of the :ref:`Connection <tntcxx_api_connection>`
                 class.

    :return: none
    :rtype: none

    **Possible errors:** none.

    **Example:**

    ..  code-block:: cpp

        client.close(conn);

.. _tntcxx_api_connection:

Connection class
----------------

..  cpp:class:: template<class BUFFER, class NetProvider> \
                class Connection;

    The ``Connection`` class is a template class that defines a connection objects
    which is required to interact with a Tarantool instance. Each connection object
    is bound to a single socket.

    Similar to a :ref:`connector client <tntcxx_api_connector>`, a connection
    object also takes the buffer and the network provider as template
    parameters, and they must be the same as ones of the client. For example:

    ..  code-block:: cpp

        //Instantiating a connector client
        using Buf_t = tnt::Buffer<16 * 1024>;
        using Net_t = DefaultNetProvider<Buf_t >;
        Connector<Buf_t, Net_t> client;

        //Instantiating connection objects
        Connection<Buf_t, Net_t> conn01(client);
        Connection<Buf_t, Net_t> conn02(client);

.. contents::
   :local:
   :depth: 1

Public types
~~~~~~~~~~~~

.. _tntcxx_api_connection_ridt:

..  cpp:type:: size_t rid_t

    The alias of the built-in ``size_t`` type. ``rid_t`` is used for entities
    that return or contain a request ID.

Public methods
~~~~~~~~~~~~~~

* :ref:`call() <tntcxx_api_connection_call>`
* :ref:`futureIsReady() <tntcxx_api_connection_futureisready>`
* :ref:`getResponse() <tntcxx_api_connection_getresponse>`
* :ref:`getError() <tntcxx_api_connection_geterror>`
* :ref:`reset() <tntcxx_api_connection_reset>`
* :ref:`ping() <tntcxx_api_connection_ping>`

.. _tntcxx_api_connection_call:

..  cpp:function:: template <class T> \
                    rid_t call(const std::string &func, const T &args)

    Executes a call of a remote stored-procedure similar to :ref:`conn:call() <net_box_call>`.
    The method returns the request ID that is used to get the response by
    :ref:`getResponse() <tntcxx_api_connection_getresponse>`.

    :param func: a remote stored-procedure name
    :param args: procedure's arguments

    :return: a request ID
    :rtype: rid_t

    **Possible errors:** none.

    **Example:**

    The following function is defined on the Tarantool instance you are
    connected to:

    ..  code-block:: lua

        box.execute("DROP TABLE IF EXISTS t;")
        box.execute("CREATE TABLE t(id INT PRIMARY KEY, a TEXT, b DOUBLE);")

        function remote_replace(arg1, arg2, arg3)
            return box.space.T:replace({arg1, arg2, arg3})
        end

    The function call can look as follows:

    ..  code-block:: cpp

        rid_t f1 = conn.call("remote_replace", std::make_tuple(5, "some_sring", 5.55));

.. _tntcxx_api_connection_futureisready:

..  cpp:function:: bool futureIsReady(rid_t future)

    Checks availability of a request ID (``future``)
    returned by any of the request methods, such as, :ref:`ping() <tntcxx_api_connection_ping>`
    and so on.

    ``futureIsReady()`` returns ``true`` if the ``future`` is available
    or ``false`` otherwise.

    :param future: a request ID

    :return: ``true`` or ``false``
    :rtype: bool

    **Possible errors:** none.

    **Example:**

    ..  code-block:: cpp

        rid_t ping = conn.ping();
        conn.futureIsReady(ping);

.. _tntcxx_api_connection_getresponse:

..  cpp:function:: std::optional<Response<BUFFER>> getResponse(rid_t future)

    The method takes a request ID (``future``) as an argument and returns
    an optional object containing a response. If the response is not ready,
    the method returns ``std::nullopt``.
    Note that for each ``future`` the method can be called only once because it
    erases the request ID from the internal map as soon as the response is
    returned to a user.

    A response consists of a header (``response.header``) and a body
    (``response.body``). Depending on success of the request execution on
    the server side, body may contain either runtime errors accessible by
    ``response.body.error_stack`` or data (tuples) accessible by
    ``response.body.data``. Data is a vector of tuples. However,
    tuples are not decoded and come in the form of pointers to the start and
    the end of MessagePacks. For details on decoding the data received, refer to
    :ref:`"Decoding and reading the data" <gs_cxx_reader>`.

    :param future: a request ID

    :return: a response object or ``std::nullopt``
    :rtype: std::optional<Response<BUFFER>>

    **Possible errors:** none.

    **Example:**

    ..  code-block:: cpp

        rid_t ping = conn.ping();
        std::optional<Response<Buf_t>> response = conn.getResponse(ping);

.. _tntcxx_api_connection_geterror:

..  cpp:function:: std::string& getError()

    Returns an error message for the last error occured during the execution of
    methods of the :ref:`Connector <tntcxx_api_connector>` and
    :ref:`Connection <tntcxx_api_connection>` classes.

    :return: an error message
    :rtype: std::string&

    **Possible errors:** none.

    **Example:**

    ..  code-block:: cpp

        int rc = client.connect(conn, address, port);

        if (rc != 0) {
            assert(conn.status.is_failed);
            std::cerr << conn.getError() << std::endl;
            return -1;
        }

.. _tntcxx_api_connection_reset:

..  cpp:function:: void reset()

    Resets a connection after errors, that is, cleans up the error message
    and the connection status.

    :return: none
    :rtype: none

    **Possible errors:** none.

    **Example:**

    ..  code-block:: cpp

        if (client.wait(conn, ping, WAIT_TIMEOUT) != 0) {
            assert(conn.status.is_failed);
            std::cerr << conn.getError() << std::endl;
            conn.reset();
        }

.. _tntcxx_api_connection_ping:

..  cpp:function:: rid_t ping()

    Prepares a request to ping a Tarantool instance.

    The method encodes the request in the `MessagePack <https://msgpack.org/>`_
    format and queues it in the output connection buffer to be sent later
    by one of :ref:`Connector's <tntcxx_api_connector>` methods, namely,
    :ref:`wait() <tntcxx_api_connector_wait>`, `waitAll() <tntcxx_api_connector_waitall>`,
    or :ref:`waitAny() <tntcxx_api_connector_waitany>`.

    Returns the request ID that is used to get the response by
    the :ref:`getResponce() <tntcxx_api_connection_getresponse>` method.

    :return: a request ID
    :rtype: rid_t

    **Possible errors:** none.

    **Example:**

    ..  code-block:: cpp

        rid_t ping = conn.ping();

Nested classes and their methods
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* :ref:`Space <tntcxx_api_connection_space>`

.. _tntcxx_api_connection_space:

Space class
^^^^^^^^^^^

..  cpp:class:: OuterScope::Space : Connection //TBD Do we need this?

    ``Space`` is a nested class of the :ref:`Connection <tntcxx_api_connection>`
    class. It is a public wrapper to access the request methods in the way
    similar to Tarantool, like, ``space[space_id].select()``,
    ``space[space_id].select()``, and so on.

    All the ``Space`` class methods listed below work in the following way:

    *   A method encodes the corresponding request in the `MessagePack <https://msgpack.org/>`_
        format and queues it in the output connection buffer to be sent later
        by one of :ref:`Connector's <tntcxx_api_connector>` methods, namely,
        :ref:`wait() <tntcxx_api_connector_wait>`, `waitAll() <tntcxx_api_connector_waitall>`,
        or :ref:`waitAny() <tntcxx_api_connector_waitany>`.

    *   A method returns the request ID that is used to get the response by
        the :ref:`getResponce() <tntcxx_api_connection_getresponse>` method.

    **Public methods**:

    * :ref:`select() <tntcxx_api_connection_select>`
    * :ref:`replace() <tntcxx_api_connection_replace>`
    * :ref:`insert() <tntcxx_api_connection_insert>`
    * :ref:`update() <tntcxx_api_connection_update>`
    * :ref:`upsert() <tntcxx_api_connection_upsert>`
    * :ref:`delete() <tntcxx_api_connection_delete>`

.. _tntcxx_api_connection_select:

..  cpp:function:: template <class T> \
                    rid_t select(const T& key, uint32_t index_id = 0, uint32_t limit = UINT32_MAX, uint32_t offset = 0, IteratorType iterator = EQ)

    Searches for a tuple or a set of tuples in the given space. The method works
    similar to :doc:`/reference/reference_lua/box_space/select` and performs the
    search against the primary index (``index_id = 0``) by default. In other
    words, ``space[space_id].select()`` equals to
    ``space[space_id].index[0].select()``.

    As all the methods of the ``rid_t`` type, this method returns just the
    request ID. To get the actual data containing the tuples selected, first
    you need to get the response by using the :ref:`getResponce() <tntcxx_api_connection_getresponse>`
    method and then :ref:`"decode" <gs_cxx_reader>` the data.

    :param const T&         key: value to be matched against the index key.
    :param uint32_t         index_id: index ID. Optional. Defaults to ``0``.
    :param uint32_t         limit: maximum number of tuples. Optional.
                                    Defaults to ``UINT32_MAX``.
    :param uint32_t         offset: number of tuples to skip. Optional.
                                    Defaults to ``0``.
    :param IteratorType     iterator: the type of iterator. Optional.
                                        Defaults to ``EQ``.

    :return: a request ID
    :rtype: rid_t

    **Possible errors:** none.

    **Example:**

    ..  code-block:: cpp

        /* Equals to space:select({key_value}, {limit = 1})*/
        uint32_t space_id = 512;
        int key_value = 5;
        uint32_t limit = 1;
        auto i = conn.space[space_id];
        rid_t select = i.select(std::make_tuple(key_value), index_id, limit, offset, iter);


..  NOTE::

    Description of other methods of the ``Space`` and ``Index`` nested classes
    listed below will be added to this document later.

Methods:

* select()
* replace()
* insert()
* update()
* upsert()
* delete()
