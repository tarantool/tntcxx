box.cfg{listen = 3301, net_msg_max=10000, readahead=163200, log_level = 7, log='tarantool.txt'}
box.schema.user.grant('guest', 'super')

box.execute("DROP TABLE IF EXISTS t;")
box.execute("CREATE TABLE t(id INT PRIMARY KEY, a TEXT, b DOUBLE);")
box.execute("insert into t values (1, 'asd', 1.123);")

function remote_replace(arg1, arg2, arg3)
    return box.space.T:replace({arg1, arg2, arg3})
end

function remote_select()
    return box.space.T:select()
end

function remote_uint()
    return 666
end

function remote_multi()
    return 'Hello', 1, 6.66
end

function get_rps()
    return box.stat.net().REQUESTS.rps
end
