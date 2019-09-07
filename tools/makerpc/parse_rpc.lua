local lpeg = require "lpeg"

local P = lpeg.P
local S = lpeg.S
local R = lpeg.R
local C = lpeg.C
local Ct = lpeg.Ct
local Cg = lpeg.Cg
local Cc = lpeg.Cc
local V = lpeg.V



local function count_lines(_,pos, parser_state)
	if parser_state.pos < pos then
		parser_state.line = parser_state.line + 1
		parser_state.pos = pos
	end
	return pos
end


local exception = lpeg.Cmt( lpeg.Carg(1) , function ( _ , pos, parser_state)
	error( "syntax error at [" .. (parser_state.file or "") .."] (" .. parser_state.line ..")" )
	return pos
end)

-- end of string
local eof = P(-1)
local newline = lpeg.Cmt((P"\n" + "\r\n") * lpeg.Carg(1) ,count_lines)
local line_comment_1 = "//" * (1 - newline) ^0 * (newline + eof)
local line_comment_2 = P"/*" * (1 - P"*/") ^0 * P"*/"
local line_comment = line_comment_1 + line_comment_2
local blank = S" \t" + newline + line_comment
local blank0 = blank ^ 0
local blanks = blank ^ 1
local alpha = R"az" + R"AZ" + "_"
local alnum = alpha + R"09"
local number = R"09" ^ 0
local name = alpha * alnum ^ 0
local class_name = "class" * blanks * name
local module_name = alpha * (alnum + P".") ^ 0
local var_type = Ct((C("class") * blanks)^-1 * C(name) * (blank0 * C("*") * blank0)^-1 * blank0)
local var_name = C(name)

local class_item = Cg(var_type * var_name * blank0 * ";" * blank0)
local args_item = Cg(var_type * var_name * blank0)
local args_sep = "," * blank0

local function insert(tbl, k, v)
	table.insert(tbl, { type = k , name = v })
	return tbl
end

parser_tbl = {"PROTOC"}
parser_tbl.PROTOC = Ct((blank + V"OUTPUT" + V"DEFINE" + V"FUNC" + V"CLASS" + V"INCLUDE" + V"IMP")^1)
--parser_tbl.PROTOC = (V"OUTPUT") ^ 1
parser_tbl.OUTPUT = Ct(Cg("output", "type") * blank0 * "=" * blank0 * Cg(name, "name") * blank0 * ";" * blank0)
parser_tbl.FUNC = Ct(Cg((C("static") * blanks) / "true", "static") ^-1 * Cg(P"void" / "function", "type") * blanks * Cg(name, "name") * blank0 * 
	"(" * blank0 * 
		Cg(lpeg.Cf(Ct"" * args_item * (args_sep * args_item)^0, insert),"value")
	* ")" * blank0 * ";" * blank0)

parser_tbl.CLASS = Ct(Cg("class", "type") * blanks * Cg(name,"name") * blank0 *
	"{" * blank0 *
		Cg(lpeg.Cf(Ct"" * class_item^1, insert),"value")
	* "}" * blank0)

parser_tbl.IMP = Ct(Cg(P"imp" / "implement", "type") * blanks * Cg(module_name, "imp_module") * blank0 * 
	"{" * 
		Cg(Ct((blank + V"DEFINE" + V"FUNC" + V"CLASS") ^ 1), "declare")
	* "}" * blank0)

parser_tbl.DEFINE = Ct(Cg(P"#define" / "define", "type") * blanks * Cg(name, "name") * blanks * Cg((1 - newline)^0, "value") * newline)
parser_tbl.INCLUDE = Ct(Cg(P"#include" / "include", "type") * blanks * P"\"" * Cg((1 - P"\"")^0, "value") * "\"" * blanks)

local parser = {}


---- class ----
-- prop_value= {1= {name=a,type= {1=int,},},2= {name=b,type= {1=int,},},3= {name=c,type= {1=string,},},},
---- function ----
-- rpc_client_test= {1= {name=a,type= {1=int,},},2= {name=b,type= {1=int,},},3= {name=v,type= {1=class,2=prop_value,},},},rpc_client_test1= {1= {name=a,type= {1=int,},},2= {name=b,type= {1=int,},},},rpc_server_test= {1= {name=c,type= {1=int,},},},

function check_class(all_class, filename)
	local cls_member_names = {}
	for cls_name, cls_tbl in pairs(all_class) do
		cls_member_names = {}
		for _index, cls_member in ipairs(cls_tbl) do
			local member_type = cls_member.type[1]
			local member_name = cls_member.name
			if member_type == "class" and not all_class[cls_member.type[2]] then
				error("undefine class "..cls_member.type[2].." in class "..cls_name.." of file "..filename)
				return
			end
			if cls_member_names[member_name] ~= nil then
				error("duplicate definition member ["..member_name.."] in class "..cls_name.." of file "..filename)
				return
			end
			cls_member_names[member_name] = 1
		end
	end
end

function check_func(all_function, all_class, filename)
	local args_member_names = {}
	for func_name, func_tbl in pairs(all_function) do
		args_member_names = {}
		for _index, func_args in ipairs(func_tbl) do
			local args_type = func_args.type[1]
			local args_name = func_args.name
			if args_type == "class" and not all_class[func_args.type[2]] then
				error("undefine class "..func_args.type[2].." in function "..func_name.." of file "..filename)
			end
			if args_member_names[args_name] ~= nil then
				error("duplicate definition member ["..args_name.."] in function"..func_name.." of file "..filename)
				return
			end
			args_member_names[args_name] = 1
		end
	end

end


function check_parse_result(dict, filename)
	local all_class = {}
	local all_function = {}
	local all_outputs = {}

	local pre_func = function(dict)
		for _, e in pairs(dict) do
			if e.type == "class" then
				all_class[e.name] = e.value
			elseif e.type == "function" then
				all_function[e.name] = e.value
			elseif e.type == "implement" then
				check_func(e.declare)
			end
		end
	end

	pre_func(dict)
	check_class(all_class, filename)
	check_func(all_function, all_class, filename)
	return all_class, all_function
end

function parser.parse_one(text, filename)
	local state = { file = filename, pos = 0, line = 1 }
	local r = lpeg.match(P(parser_tbl) * -1 + exception, text, 1, state)
	--local r = lpeg.match(parser_tbl.PROTOC * -1, text)
	check_parse_result(r, filename)
	return r
	-- return deal_all(r, filename)
end

function list_dir(path)
	local files = {}
	local i = 1
	os.execute("ls -lS " .. path .. " > _temp.txt")
	io.input("_temp.txt")
	for line in io.lines() do
		filename = string.match(line, "%w[%w_]+%.decl")
		if filename ~= nil then
			files[i] = filename
			i = i + 1
		end
	end
	os.execute("rm _temp.txt")
	return files
end


function parser.parse_file(filename)
	local f = assert(io.open(filename, "r"))
	local buffer = f:read "*a"
	f:close()
	return parser.parse_one(buffer,filename)
end

function merge_table(dest_table, src_table)
	for _, e in src_table do
		table.insert(dest_table, e)
	end
end

function parser.parse_dir(path)
	local listfiles = list_dir(path)
	local all = {}
	for _, file in ipairs(listfiles) do
		local filename = path..file
		local f = assert(io.open(filename, "r"))
		local buffer = f:read "*a"
		f:close()
		local one_res = parser.parse_one(buffer, file)
		table.insert(all, {
			["file"] = file, 
			["content"] = one_res})
	end
	return all
end


local client_patterns = {
  	rpc_internal = "fs_rpc_call",
  	rpc_gamed = "fs_rpc_call_gamed",
  	rpc_dbd = "fs_rpc_call",
	rpc_ls = "fs_ls_rpc_call",
	rpc_gs = "fs_gs_rpc_call",
}

function parser.server_file_lpc_serial(content, filename)
	local do_serial

	local is_internal_rpc_declare = function(func_name)
		for pattern, rpc_call_name in pairs(client_patterns) do
			local s,e = string.find(func_name, pattern.."_.*")	
			if s == 1 then	
				return rpc_call_name
			end
		end
	end

	local serial_func_table = {
		["class"]  =  function(e) 
			local member_table = {}
			for _, v in ipairs(e.value) do
				local type_str = v.type[1]..(v.type[2] or "")
				table.insert(member_table, string.format("\t%s %s;", type_str, v.name))
			end
			return string.format("class %s {\n%s\n}\n", e.name, table.concat(member_table, "\n"))
		end,
		["function"] = function(e)
			local args_tbl = {}
			local args_name_tbl = {}
			for i, v in ipairs(e.value) do
				local type_str = v.type[1]..(v.type[2] or "")
				table.insert(args_tbl, string.format("%s %s", type_str, v.name))
				table.insert(args_name_tbl, v.name)
			end
			local server_declare = string.format("void %s(%s);\n", e.name, table.concat(args_tbl, ", "))
			table.insert(args_name_tbl, 2, string.format("GET_%s", string.upper(e.name)))
			local rpc_call_name = is_internal_rpc_declare(e.name)
			if string.find(e.name, "rpc_client_.*") then
				return string.format("void %s(%s)  { \n\tfs_rpc_call(%s); \n}\n", e.name, table.concat(args_tbl, ", "), table.concat(args_name_tbl, ", "))
			elseif rpc_call_name then
				return server_declare..string.format("void %s_send(%s) { \n\t%s(%s); \n}\n", e.name, table.concat(args_tbl, ", "), rpc_call_name, table.concat(args_name_tbl, ", "))
			end
			return server_declare
		end,
		["define"] = function(e)
			return string.format("#define %s %s\n", e.name, e.value)
		end,
		["include"] = function(e)
			return string.format("#include %s", e.value)
		end,
		["implement"] = function(e)
			return do_serial(e.declare)
		end
	}

	do_serial = function(dict)
		local result = {}
		for _, e in ipairs(dict) do
			local func = serial_func_table[e.type]
			if func then 
				local r = func(e) 
				if r then
					table.insert(result, r)
				end
			end
		end
		return table.concat(result, "\n")
	end

	local filename, _ = string.gsub(filename, "%.", "_")
	local define_macros = string.format("RC_RPC_%s", string.upper(filename))

	local serial_tbl = {}
	table.insert(serial_tbl, string.format("#ifndef %s", define_macros))
	table.insert(serial_tbl, string.format("#define %s", define_macros))
	table.insert(serial_tbl, "#include \"/rc/rpc/rpc_id.h\"\n")
	table.insert(serial_tbl, "#include \"/rc/rpc/rpc_id_decl.h\"\n")
	table.insert(serial_tbl, do_serial(content))
	table.insert(serial_tbl, string.format("#endif // %s\n", define_macros))

	return table.concat(serial_tbl, "\n")
end

function parser.client_file_lpc_serial(content, filename)
	local do_serial

	local is_internal_rpc_declare = function(func_name)
		for pattern, rpc_call_name in pairs(client_patterns) do
			local s,e = string.find(func_name, pattern.."_.*")	
			if s == 1 then	
				return rpc_call_name
			end
		end
	end

	local serial_func_table = {
		["class"]  =  function(e) 
			local member_table = {}
			for _, v in ipairs(e.value) do
				local type_str = v.type[1]..(v.type[2] or "")
				table.insert(member_table, string.format("\t%s %s;", type_str, v.name))
			end
			return string.format("class %s {\n%s\n}\n", e.name, table.concat(member_table, "\n"))
		end,
		["function"] = function(e)
			local args_tbl = {}
			local args_name_tbl = {}
			for i, v in ipairs(e.value) do
				local type_str = v.type[1]..(v.type[2] or "")
				table.insert(args_tbl, string.format("%s %s", type_str, v.name))
				table.insert(args_name_tbl, v.name)
			end

			-- 替换第一个
			args_tbl[1] = "int vfd"
			args_name_tbl[1] = "vfd"
			local name = "v_"..e.name
			local server_declare = string.format("void %s(%s);\n", e.name, table.concat(args_tbl, ", "))
			if string.find(e.name, "rpc_server_.*") then
				-- 插入第二个参数
				table.insert(args_name_tbl, 2, string.format("GET_%s", string.upper(e.name)))
				return string.format("void %s(%s)  { \n\tfs_v_client_rpc_server_call(%s); \n}\n", name, table.concat(args_tbl, ", "), table.concat(args_name_tbl, ", "))
			end
			return server_declare
		end,
		["define"] = function(e)
			return string.format("#define %s %s\n", e.name, e.value)
		end,
		["include"] = function(e)
			return string.format("#include %s", e.value)
		end,
		["implement"] = function(e)
			return do_serial(e.declare)
		end
	}

	do_serial = function(dict)
		local result = {}
		for _, e in ipairs(dict) do
			local func = serial_func_table[e.type]
			if func then 
				local r = func(e) 
				if r then
					table.insert(result, r)
				end
			end
		end
		return table.concat(result, "\n")
	end

	local filename, _ = string.gsub(filename, "%.", "_")
	local define_macros = string.format("RC_RPC_%s", string.upper(filename))

	local serial_tbl = {}
	table.insert(serial_tbl, string.format("#ifndef %s", define_macros))
	table.insert(serial_tbl, string.format("#define %s", define_macros))
	table.insert(serial_tbl, "#include \"/rc/rpc/rpc_id.h\"\n")
	table.insert(serial_tbl, "#include \"/rc/rpc/rpc_id_decl.h\"\n")
	table.insert(serial_tbl, do_serial(content))
	table.insert(serial_tbl, string.format("#endif // %s\n", define_macros))

	return table.concat(serial_tbl, "\n")
end
return parser


