box.cfg{listen = 3301, net_msg_max=10000, readahead=163200, log='tarantool.txt'}
box.schema.user.grant('guest', 'super', nil, nil, {if_not_exists=true})

if box.space.t then box.space.t:drop() end
s = box.schema.space.create('T')
s:format{{name='id',type='integer'},{name='a',type='string'},{name='b',type='number'}}
s:create_index('primary')
s:replace{1, 'asd', 1.123}

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

function bench_func(...)
    return {...}
end

function get_rps()
    return box.stat.net().REQUESTS.rps
end
