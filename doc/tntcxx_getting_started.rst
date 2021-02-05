.. _getting_started-cxx:

--------------------------------------------------------------------------------
Connecting from C++
--------------------------------------------------------------------------------

.. _getting_started-cxx-pre-requisites:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Pre-requisites
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Before we proceed:

#. <Installation step: either the static content here or a link to tntcxx reamdme -- TBD>

#. :ref:`Start <getting_started_db>` Tarantool (locally or in Docker)
   and make sure that you have created and populated a database as we suggested
   :ref:`earlier <creating-db-locally>`:

   .. code-block:: lua

       box.cfg{listen = 3301}
       s = box.schema.space.create('tester')
       s:format({
                {name = 'id', type = 'unsigned'},
                {name = 'band_name', type = 'string'},
                {name = 'year', type = 'unsigned'}
                })
       s:create_index('primary', {
                type = 'hash',
                parts = {'id'}
                })
       s:create_index('secondary', {
                type = 'hash',
                parts = {'band_name'}
                })
       s:insert{1, 'Roxette', 1986}
       s:insert{2, 'Scorpions', 2015}
       s:insert{3, 'Ace of Base', 1993}

   .. IMPORTANT::

       Please do not close the terminal window
       where Tarantool is running -- you'll need it soon.

#. In order to connect to Tarantool as an administrator, reset the password
   for the ``admin`` user:

   .. code-block:: lua

       box.schema.user.passwd('pass')

.. _getting_started-cxx-connecting:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Connecting to Tarantool
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To get connected to the Tarantool server, write a simple C++ program:

.. code-block:: c

   <code-block body TBD>



.. _getting_started-cxx-manipulate:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Manipulating the data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

<TOC> or a bunch of refs

.. _getting_started-cxx-insert:

********************************************************************************
Inserting data
********************************************************************************

To insert a tuple into a space:

.. code-block:: c

   <code-block body TBD>



.. _getting_started-cxx-others:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
<Other important points for C++ connector to start & use>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

<TBD>
