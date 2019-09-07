local parser = require("parse_rpc")


local function table_index(table, elem)
	for i, v in ipairs(table) do
		if v.name == elem then
			return i - 1
		end
	end
	return -1
end

local type_2_value_meta = {
	['int'] = 0,
	['int32_t'] = 0,
	['string'] = 1, 
	['class'] = 2,
	['object'] = 3,
	['mixed'] = 4,
	['buffer'] = 5,
	['int8_t'] = 6,
	['int16_t'] = 7,
	['float'] = 8,
	['double'] = 9,
	['int64_t'] = 10,
}

local function parse_type(cls_table, ...)
    local arg = {...}
	local cls_index = table_index(cls_table, arg[1])
	local field_type = arg[1]
	if cls_index >= 0 then
		field_type = "class"
	end
	return field_type, type_2_value_meta[field_type], cls_index, ((arg[2] == "*") and -1 or -2)
end


local function func_trans(fl, all_cls_table, func_table, func_id)
	--print("function : "..table.show(func_table))
	local c_imp = (func_table.c_imp == "true") and 1 or 0
	local modname = func_table.module or ""
	table.insert(fl, string.format("function_id:%d,c_imp:%d,arg_count:%d,module:%s,function_name:%s", 
		func_id, c_imp, #func_table.value, modname, func_table.name))

	for _, v in ipairs(func_table.value) do
		local field_type, field_type_value, class_index, array = parse_type(all_cls_table, unpack(v.type))
		table.insert(fl, string.format("arg_type:%d,class_index:%d,array:%d,arg_name:%s",
			field_type_value, class_index, array, v.name))
	end
end

local function translate_one(all_functions, all_classes, all_modules, result, imp_module)
	for _, t in ipairs(result) do
		if imp_module then
			t.module, _ = string.gsub(imp_module, "%.", "/")
			t.c_imp =  imp_module == "c" and "true" or "false"
			all_modules[t.module] = true
		else
			t.module = "no_such_module"
		end
		if t.type == "function" then
			table.insert(all_functions, t)
		elseif t.type == "class" then
			table.insert(all_classes, t)
		elseif t.type == "implement" then
			translate_one(all_functions, all_classes, all_modules, t.declare, t.imp_module)
		end
	end
end

local function cls_trans(fl, all_cls_table, cls_table)
	local c_imp = (cls_table.c_imp == "true") and 1 or 0
	table.insert(fl, string.format("field_count:%d,c_imp:%d,class_name=%s", #cls_table.value, c_imp, cls_table.name))
	for _, v in ipairs(cls_table.value) do
		local field_name = v.name
		local field_type, field_type_value, class_index, array = parse_type(all_cls_table, unpack(v.type))
		table.insert(fl, string.format("field_type:%d,class_index:%d,array:%d,field_name:%s", 
			field_type_value, class_index, array, field_name))
	end
end

local function parse_static_rpc_table(file)
	local tbl = {}
	local tmp_f, err = io.open(file)
	if err then
		return tbl
	end
	tmp_f:close()

	local static_rpc_table = parser.parse_file(file)
	for _, e in ipairs(static_rpc_table) do
		if e.type == "define" then
			tbl[e.name] = tonumber(e.value)
		end
	end
	return tbl
end



local function translate(parse_result_tbl, input, output)
	local all_functions = {}
	local all_classes = {}
	local all_modules = {}
	local rpc_id_fl = {}
	local rpc_cfg_fl = {}
	local old_pid_table = {}
	local rpc_static_begin_pid = 1

	local rpc_static_pid_tbl = parse_static_rpc_table(input.."static_rpc_id.h")
	for k, v in pairs(old_pid_table) do
		rpc_static_pid_tbl[k] = v
	end
	for _, v in pairs(rpc_static_pid_tbl) do
		if v >= rpc_static_begin_pid then
			rpc_static_begin_pid = v
		end
	end

	for _, t in ipairs(parse_result_tbl) do
		translate_one(all_functions, all_classes, all_modules, t.content)
	end

	-- print(table.show(all_functions))

	table.insert(rpc_cfg_fl, string.format("class_table_num:%d", #all_classes))
	for _, t in ipairs(all_classes) do
		cls_trans(rpc_cfg_fl, all_classes, t)
	end

	table.insert(rpc_cfg_fl, string.format("function_table_num:%d", #all_functions))
	local pid_tbl = {}
	for i, t in ipairs(all_functions) do
		local pid = rpc_static_pid_tbl[string.upper(t.name)]
		if pid == nil then
			rpc_static_begin_pid = rpc_static_begin_pid + 1
			pid = rpc_static_begin_pid 
		end
		func_trans(rpc_cfg_fl, all_classes, t, pid)
		pid_tbl[pid] = string.upper(t.name)
	end

	local sorted_pid = {}
	for pid, _ in pairs(pid_tbl) do
		table.insert(sorted_pid, pid)
	end
	table.sort(sorted_pid)
	for _, pid in pairs(sorted_pid) do
		local defname = pid_tbl[pid]
		table.insert(rpc_id_fl, string.format("#define %s %d", defname, pid))
	end

	-- rpc_id.h
	table.insert(rpc_id_fl, 1, [[ 
#ifndef _FS_RPC_ID_
#define _FS_RPC_ID_
]]);
	table.insert(rpc_id_fl, "\n#endif /*_FS_RPC_ID_*/\n");
	local rpc_id_content = table.concat(rpc_id_fl, "\n")
	sf = io.open(output.."rpc_id.h", "w")
	sf:write(rpc_id_content)
	io.close(sf)

	-- rpc.cfg
	local rpc_cfg_content = table.concat(rpc_cfg_fl, "\n")
	sf = io.open(output.."rpc.cfg", "w")
	sf:write(rpc_cfg_content)
	io.close(sf)
end


--local input_dir = "/home/linzhijun/work/coc/logic/rc/rpc_decl/"
--local output_dir = "./"
local input_dir = arg[1]
local output_dir = arg[2]

if #arg ~= 2 then
	print("\
usage:\
lua trans_rpc_cfg.lua input_dir output_dir\n")
	os.exit(-1)
end

local test_f, err = io.open(input_dir)
if err then
	print(string.format("error: directory %s not exists", input_dir))
	os.exit(-1)
end
test_f:close()

test_f, err = io.open(output_dir)
if err then
	print(string.format("error: directory %s not exists", output_dir))
	os.exit(-1)
end
test_f:close()

local result = parser.parse_dir(input_dir) 
translate(result, input_dir, output_dir)
