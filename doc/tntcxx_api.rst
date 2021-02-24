.. _tntcxx_api:


Tarantool C++ connector API
============================

.. //This is a draft template for documenting Tarantool C++ connector API.

.. // TBD -- Introduction

.. //TBD -- ToC


.. _tntcxx_api_connector:

class ``Connector``
--------------------

.. //description TBD

Methods:

* :ref:`connect() <tntcxx_api_connector_connect>`
* :ref:`wait() <tntcxx_api_connector_wait>`
* :ref:`waitAll() <tntcxx_api_connector_waitall>`
* :ref:`waitAny() <tntcxx_api_connector_waitany>`
* :ref:`close() <tntcxx_api_connector_close>`

.. _tntcxx_api_connector_connect:

.. c:function:: int connect(Connection<BUFFER, NetProvider>& conn, const std::string_view& addr, unsigned port)

   Connects to a Tarantool instance that is listening on ``addr:port``.

   :param conn: connection object <cross-ref to corresp. topic>.
   :param addr: URL of a host where a Tarantool instance is running.
   :param port: a port that a Tarantool instance is listening to.

   :return: connection status code.
   :rtype: <TBD>

   **Possible errors:** <TBD>

   **Complexity factors:** <TBD>

   **Example:**

   ..  code-block:: c

       using Buf_t = tnt::Buffer<16 * 1024>;
       using Net_t = DefaultNetProvider<Buf_t >;

       Connector<Buf_t, Net_t> client;
       Connection<Buf_t, Net_t> conn(client);

       int rc = client.connect(conn, "127.0.0.1", 3301);

.. _tntcxx_api_connector_wait:

.. c:function::  wait()

   <description>.

   :param <param>: <description>.


   :return: <description>.
   :rtype: <description>

   **Possible errors:** <description>

   **Complexity factors:** <description>

   **Example:**

   ..  code-block:: c

       <code example>

.. _tntcxx_api_connector_waitall:

.. c:function::  waitAll()

   <description>.

   :param <param>: <description>.


   :return: <description>.
   :rtype: <description>

   **Possible errors:** <description>

   **Complexity factors:** <description>

   **Example:**

   ..  code-block:: c

       <code example>

.. _tntcxx_api_connector_waitany:

.. c:function::  waitAny()

   <description>.

   :param <param>: <description>.


   :return: <description>.
   :rtype: <description>

   **Possible errors:** <description>

   **Complexity factors:** <description>

   **Example:**

   ..  code-block:: c

       <code example>

.. _tntcxx_api_connector_close:

.. c:function::  close()

   <description>.

   :param <param>: <description>.


   :return: <description>.
   :rtype: <description>

   **Possible errors:** <description>

   **Complexity factors:** <description>

   **Example:**

   ..  code-block:: c

       <code example>


.. _tntcxx_api_connection:

class ``Connection``
--------------------

.. //description TBD

Methods:

call()
futureIsReady()
getResponse()
getError()
reset()
ping()
select()
replace()
insert()
delete()
update()
upsert()

