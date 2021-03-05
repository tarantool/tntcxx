box.cfg{listen = 3301 }
box.schema.user.grant('guest', 'super')

box.execute("DROP TABLE IF EXISTS t;")
box.execute("CREATE TABLE t(id INT PRIMARY KEY, a TEXT, b DOUBLE);")

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
