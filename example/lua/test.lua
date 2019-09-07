require "RpcLua"
require "RpcMeta"

local RpcMetas = RpcMeta.getRpcMetas()

local f = io.open("../rpc.cfg", "r")
local Buf = f:read("*a")
f:close()

local RPC = RpcLua.create_from_buf(Buf, RpcMetas)
print(RPC)

if not RPC then
    print("init rpc cfg failed")
end

function isequal(a, b)
    if type(a) ~= type(b) then
        return false
    end

    t = type(a)
    if t == "table" then
        if table.getn(a) ~= table.getn(b) then
            return false
        end

        for k, v in pairs(a) do
            if isequal(a[k], b[k]) == false then
                return false
            end
        end

        return true
    else
        return a == b
    end
end

function var_dump(data, max_level, prefix)  
    if type(prefix) ~= "string" then  
        prefix = ""  
    end  
    if type(data) ~= "table" then  
        print(prefix .. tostring(data))  
    else  
        print(data)  
        if max_level ~= 0 then  
            local prefix_next = prefix .. "    "  
            print(prefix .. "{")  
            for k,v in pairs(data) do  
                io.stdout:write(prefix_next .. k .. " = ")  
                if type(v) ~= "table" or (type(max_level) == "number" and max_level <= 1) then  
                    print(v)  
                else  
                    if max_level == nil then  
                        var_dump(v, nil, prefix_next)  
                    else  
                        var_dump(v, max_level - 1, prefix_next)  
                    end  
                end  
            end  
            print(prefix .. "}")  
        end  
    end  
end  
  
function dump(data, max_level)  
    var_dump(data, max_level or 5)  
end


mail_t1 = {

	glbkey = "aasddfff",
	type = 1,
	sender = {
		uid = 10018,
		honor = 9999,
		name = "MyNameIs007",
		legion_id = "2307_12345234",
		legion_name = "LEGION007",
		legion_icon = "21",
	},
	["time"] = 1234566,
	message = {
		type = 1,
		msg = "this is a mail message",
	},
	reward = "",
	isPicked = 0,
	from = 1,
}
mail_t2 = {

	glbkey = "aasddfff222",
	type = 2,
	sender = {
		uid = 10018,
		honor = 9999,
		name = "MyNameIs007",
		legion_id = "2307_12345234",
		legion_name = "LEGION007",
		legion_icon = "21",
	},
	["time"] = 1234566,
	message = {
		type = 1,
		msg = "this is a mail message",
	},
	reward = "",
	isPicked = 0,
	from = 1,
}

move_t = {
	speed = 877,
	path = {
		{ x = 1, y = 1},
		{ x = 2, y = 1},
		{ x = 3, y = 1},
		{ x = 4, y = 1},
	},
}

function test_equal(a, b, err)
    if not isequal(a, b) then
        print(string.format("[%s] not equal", err))
        dump(a)
        print("-----------")
        dump(b)
        assert(false)
        return false
    end

    return true
end

function test()
    local rpc_table = RPC:create_rpc_table()
    print(rpc_table)
    local pid = nil

    for _pid, _funcobj in pairs(rpc_table["function"]) do
        if _funcobj["name"] == "rpc_client_mail_list" then
            pid = _pid
            break
        end
    end

    local args = {{mail_t1, mail_t2}, 888}
    local pack_result = RPC:pack(pid, args)
    local unpack_size, unpack_pid, unpack_args = RPC:unpack(pack_result, 0)

    test_equal(string.len(pack_result), unpack_size, "protocol size")
    test_equal(pid, unpack_pid, "protocol id")
    test_equal(args, unpack_args, "protocol args")

    -- 测试C层unpack
    for _pid, _funcobj in pairs(rpc_table["function"]) do
        if _funcobj["name"] == "rpc_server_move" then
            pid = _pid
            break
        end
    end

    local args = {move_t,}
    local pack_result = RPC:pack(pid, args)
    local unpack_size, unpack_pid, unpack_args = RPC:unpack(pack_result, 0)

    print("pack size=", string.len(pack_result))
    print("unpack size=", unpack_size)
    
    test_equal(string.len(pack_result), unpack_size, "protocol size")
    test_equal(pid, unpack_pid, "protocol id")
    test_equal(nil, unpack_args, "protocol args")

    local num = 1230000
    local bytes = RpcLua.int32_to_bytes(num)
    local num2 = RpcLua.bytes_to_int32(bytes)
    test_equal(num, num2, "int-bytes trans")
end

test()
print("test ok")

