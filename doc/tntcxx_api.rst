
Tarantool C++ connector API
===========================

.. // TBD -- Introduction


.. contents::
   :local:
   :depth: 1

.. _tntcxx_api_connector:

class ``Connector``
-------------------

.. //description TBD

Methods:

* :ref:`connect() <tntcxx_api_connector_connect>`
* :ref:`wait() <tntcxx_api_connector_wait>`
* :ref:`waitAll() <tntcxx_api_connector_waitall>`
* :ref:`waitAny() <tntcxx_api_connector_waitany>`
* :ref:`close() <tntcxx_api_connector_close>`

.. _tntcxx_api_connector_connect:

..  c:function:: int connect(Connection<BUFFER, NetProvider>& conn, const std::string_view& addr, unsigned port)

    Connects to a Tarantool instance. If a host doesn't reply within a timeout
    period (2 seconds by default), an error is returned.

    :param conn: connection object of the ``Connection`` class.
    :param addr: address of the host where a Tarantool instance is running.
    :param port: port that a Tarantool instance is listening to.

    :return: ``0`` on success.
    :return: ``-1`` otherwise. An error message is set that can be get by
             the ``connection.getError()`` method.
    :rtype: int

    **Possible errors:**

    *   connection timeout
    *   refused to connect (incorrect address or/and port)
    *   system errors. For example, a socket cannot be created or any of the
        system calls failed (``fcntl``, ``select``, ``send``, ``receive``).

    **Example:**

    ..  code-block:: cpp

        using Buf_t = tnt::Buffer<16 * 1024>;
        using Net_t = DefaultNetProvider<Buf_t >;

        Connector<Buf_t, Net_t> client;
        Connection<Buf_t, Net_t> conn(client);

        int rc = client.connect(conn, "127.0.0.1", 3301);

.. _tntcxx_api_connector_wait:

..  c:function:: int wait(Connection<BUFFER, NetProvider>& conn, rid_t future, int timeout = 0)

    The main method responsible for sending requests and checking response
    readiness. Once a response for the specified request is ready,
    ``wait()`` terminates.

    :param conn: connection object of the ``Connection`` class.
    :param future: request ID returned by the corresponding method of the ``Connection`` class.
    :param timeout: waiting timeout, milliseconds. Optional. Defaults to ``0``
                    which means the future is polled until it's ready.

    :return: ``0`` on receiving a response.
    :return: ``-1`` otherwise. An error message is set that can be get by
             the ``connection.getError()`` method.
    :rtype: int

    **Possible errors:**

    *   connection timeout
    *   connection is broken (for example, closed)
    *   epoll is failed.

    **Complexity factors:** <description>

    **Example:**

    ..  code-block:: cpp

        rid_t ping = conn.ping();

        while (! conn.futureIsReady(ping)) {
            if (client.wait(conn, ping, WAIT_TIMEOUT) != 0) {
                assert(conn.status.is_failed);
                std::cerr << conn.getError() << std::endl;
                conn.reset();
            }
        }

.. _tntcxx_api_connector_waitall:

..  c:function:: void waitAll(Connection<BUFFER, NetProvider>& conn, rid_t *futures, size_t future_count, int timeout = 0)

    Responsible for sending several requests and checking response

    :param conn: connection object of the ``Connection`` class.
    :param *futures: request IDs returned by the corresponding methods of the ``Connection`` class.
    :param future_count: number of requests to send.
    :param timeout: waiting timeout, milliseconds. Optional. Defaults to ``0``
                    which means the futures are polled until they are ready.

    :return: none
    :rtype: none

    **Possible errors:** <description>

    **Complexity factors:** <description>

    **Example:**

    ..  code-block:: cpp

        rid_t futures[2];
        futures[0] = replace;
        futures[1] = select;

        client.waitAll(conn, (rid_t *) &futures, 2);
        for (int i = 0; i < 2; ++i) {
            assert(conn.futureIsReady(futures[i]));
            response = conn.getResponse(futures[i]);
            assert(response != std::nullopt);;
        }

.. _tntcxx_api_connector_waitany:

..  c:function:: Connection<BUFFER, NetProvider>* waitAny(int timeout = 0)

    Returns the first connection that has received a response.

    :param timeout: waiting timeout, milliseconds. Optional. Defaults to ``0``
                    which means the futures are polled until they are ready.

    :return: connection object of the ``Connection`` class.
    :rtype: Connection<BUFFER, NetProvider>*

    **Possible errors:** <description>

    **Complexity factors:** <description>

    **Example:**

    ..  code-block:: cpp

        Connection<Buf_t, Net_t> *first = client.waitAny(WAIT_TIMEOUT);
        if (first == &conn) {
            assert(conn.futureIsReady(f1));
        } else {
            assert(another.futureIsReady(f2));
        }

.. _tntcxx_api_connector_close:

..  c:function:: void close(Connection<BUFFER, NetProvider>& conn)

    Closes the connection established earlier by
    the :ref:`connect() <tntcxx_api_connector_connect>` method.

    :param conn: connection object of the ``Connection`` class.

    :return: none
    :rtype: none

    **Possible errors:** <description>

    **Complexity factors:** <description>

    **Example:**

    ..  code-block:: cpp

        client.close(conn);


.. _tntcxx_api_connection:

class ``Connection``
--------------------

..  NOTE::

    Description of the ``Connection`` class and its methods listed below will
    be added to this document later.

Methods:

* call()
* futureIsReady()
* getResponse()
* getError()
* reset()
* ping()
* select()
* replace()
* insert()
* delete()
* update()
* upsert()
