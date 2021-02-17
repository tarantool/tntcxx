.. _tntcxx_api:


Tarantool C++ connector API
============================

.. //This is a draft template for documenting Tarantool C++ connector API.

.. // TBD -- Introduction

.. //TBD -- ToC

List of public classes and methods
-----------------------------------

.. //TBD -- currently just a flat list for understanding the scope. Formatting etc. - TBD

* class Connector

  connect()
  wait()
  waitAll()
  waitAny()
  close()

* class Connection

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


.. _tntcxx_api_connector:

class Connector
----------------

.. //description TBD


..  class:: Connector

    ..  method:: int connect(Connection<BUFFER, NetProvider> &conn, const std::string_view& addr, unsigned port)

        Connects to a Tarantool instance that is listening ``addr:port``.

        :param conn: connection object <cross-ref to corresp. topic>.
        :param addr: URL of a host where a Tarantool instance is running.
        :param port: a port that a Tarantool instance is listening to.

        :return: connection status code.
        :rtype: <TBD>

        **Possible errors:** <TBD>

        **Complexity factors:** <TBD>

        **Example:**

        ..  code-block:: c

            const char *address = "127.0.0.1";
            int port = 3301;

            using Buf_t = tnt::Buffer<16 * 1024>;
            using Net_t = DefaultNetProvider<Buf_t >;

            Connector<Buf_t, Net_t> client;
            Connection<Buf_t, Net_t> conn(client);

            int rc = client.connect(conn, address, port);


