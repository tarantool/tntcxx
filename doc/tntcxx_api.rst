
Tarantool C++ connector API
===========================

The official C++ connector for Tarantool is located `here <https://github.com/tarantool/tntcxx/>`_.

It is not supplied as part of the Tarantool repository and requires additional
actions for usage.
The connector itself is a header-only library, and, as such, doesn't require
installation and building. All you need is to clone the connector
source code and embed it in your C++ project. See the :doc:`C++ connector Getting started </getting_started/getting_started_cxx>`
article for details and examples.

Below is the description of the connector public API.

.. contents::
   :local:
   :depth: 1

.. _tntcxx_api_connector:

Сlass ``Connector``
-------------------

Represents a connector client that can handle many connections to
Tarantool instances asynchronously.

To instantiate a client, you should specify the buffer and the network provider
implementations as template parameters.

Class signature:

..  code-block:: cpp

    template<class BUFFER, class NetProvider = DefaultNetProvider<BUFFER>>
    class Connector;

You can either implement your own buffer or network provider or use the default
ones. The default connector instantiation looks as follows:

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
    it returns ``-1``. Then, ``connection.getError()`` gives the error message.

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

    You should prepare a request beforehand by using
    a method of the :ref:`Connection <tntcxx_api_connection>` class, such as
    ``ping()``, ``select()``, ``replace()``, and so on, which encodes the request
    in the `MessagePack <https://msgpack.org/>`_ format and saves it in
    the output connection buffer.

    ``wait()`` sends the request and is polling the ``future`` for the response
    readiness. Once the response is ready, ``wait()`` returns ``0``.
    If at ``timeout`` the response isn't ready or another error occurs,
    it returns ``-1``. Then, ``connection.getError()`` gives the error message.
    ``timeout = 0`` means the method is polling the ``future`` until the response
    is ready.

    :param conn: object of the :ref:`Connection <tntcxx_api_connection>`
                 class.
    :param future: request ID returned by a request method of
                    the :ref:`Connection <tntcxx_api_connection>` class, such as,
                    ``ping()``, ``select()``, ``replace()``, and so on.
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
    Exceeding the timeout leads to an error; ``connection.getError()`` gives
    the error message.
    ``timeout = 0`` means the method is polling the ``futures``
    until all the responses are ready.

    :param conn: object of the :ref:`Connection <tntcxx_api_connection>`
                 class.
    :param *futures: array with the request IDs returned by request
                     methods of the :ref:`Connection <tntcxx_api_connection>`
                     class, such as, ``ping()``, ``select()``, ``replace()``,
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
    ``nullptr``. Then, ``connection.getError()`` gives the error message.
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

Сlass ``Connection``
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
