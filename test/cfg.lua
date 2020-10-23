box.cfg{listen = 3301 }
box.schema.user.grant('guest', 'super')

box.execute("DROP TABLE IF EXISTS t;")
box.execute("CREATE TABLE t(id INT PRIMARY KEY, a TEXT, b DOUBLE);")
