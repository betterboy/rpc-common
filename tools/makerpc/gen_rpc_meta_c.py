#encoding=utf-8

import re
import os
import sys

def read_rpc_cfg(path):
    class_table = []
    function_table = {}
    f = open(path, "r")
    line = f.readline().strip()
    class_table_num = int(re.match(r"class_table_num:(\d+)", line).group(1))

    class_pattern = re.compile(r"field_count:(\d+),c_imp:(\d+),class_name=(\S+)")
    field_pattern = re.compile(r"field_type:(\d+),class_index:([-]?\d+),array:([-]?\d+),field_name:(\S+)")
    function_pattern = re.compile(r"function_id:(\d+),c_imp:(\d+),arg_count:(\d+),module:(\S+),function_name:(\S+)")
    function_arg_pattern = re.compile(r"arg_type:(\d+),class_index:([-]?\d+),array:([-]?\d+)")

    first = False
    for i in xrange(class_table_num):
        line = f.readline().strip()
        match = class_pattern.match(line)

        cls = {
            "field_count": int(match.group(1)),
            "c_imp": int(match.group(2)),
            "class_name": match.group(3),
            "args": [],
        }
        class_table.append(cls)

        for j in xrange(cls["field_count"]):
            line = f.readline().strip()
            match = field_pattern.match(line)

            arg = {
                "field_type": int(match.group(1)),
                "class_index": int(match.group(2)),
                "array": int(match.group(3)),
                "field_name": match.group(4),
            }
            cls["args"].append(arg)


    line = f.readline().strip()
    function_table_num = int(re.match(r"function_table_num:(\d+)", line).group(1))

    for i in xrange(function_table_num):
        line = f.readline().strip()
        match = function_pattern.match(line)

        fun = {
            "function_id": int(match.group(1)),
            "c_imp": int(match.group(2)),
            "arg_count": int(match.group(3)),
            "module": match.group(4),
            "function_name": match.group(5),
            "args": [],
        }
        function_table[fun["function_id"]] = fun

        for j in xrange(fun["arg_count"]):
            line = f.readline().strip()
            match = function_arg_pattern.match(line)

            arg = {
                "arg_type": int(match.group(1)),
                "class_index": int(match.group(2)),
                "array": int(match.group(3)),
            }
            fun["args"].append(arg)

    f.close()
    return class_table, function_table

def gen_to_c(class_table, function_table, path):
    s =  '#include "rpc.h"\n'
    s += '#include "rpc_meta.h"\n'

    # rpc_field_meta_t
    for cls in class_table:
        if cls["c_imp"] == 0: continue

        s += 'static rpcFieldMeta %s_fields[] = {\n' % (cls["class_name"])

        args = cls["args"]
        for field in args:
            s += '    {"%s", offsetof(struct %s, %s), },\n' % (field["field_name"], cls["class_name"], field["field_name"])

        s += '};\n\n'

    #rpc_class_meta_t
    s += 'rpc_class_meta_t auto_rpc_class_metas[] = {\n'

    for cls in class_table:
        if cls["c_imp"] == 0: continue

        s += '    {"%s", sizeof(struct %s), %d, %s_fields, },\n' % (cls["class_name"], cls["class_name"], cls["field_count"], cls["class_name"])

    s += '};\n\n'

    #function
    for function_id, fun in function_table.iteritems():
        if fun["c_imp"] == 0: continue
        if fun["function_name"].startswith("rpc_client"): continue

        if fun["arg_count"] == 1:
            s += 'extern void %s(int);\n\n' % (fun["function_name"])
        else:
            class_index = fun["args"][1]["class_index"]
            cls = class_table[class_index]
            s += 'extern void %s(int, struct %s*);\n\n' % (fun["function_name"], cls["class_name"])
    

    #rpc_function_meta_t
    s += 'rpcFunctionMeta auto_rpc_function_metas[] = {\n'

    for function_id, fun in function_table.iteritems():
        if fun["c_imp"] == 0: continue
        if fun["function_name"].startswith("rpc_client"): continue

        s += '{"%s", (rpc_c_func_t)%s,},\n' % (fun["function_name"], fun["function_name"])
    
    s += '{NULL, NULL,},\n';
    s += '};\n\n';

    # rpc_meta_t

    s += 'rpc_meta_t auto_rpc_metas = {\n'
    s += '    auto_rpc_class_metas,\n'
    s += '    auto_rpc_function_metas,\n'
    s += '};\n'

    f = open(path, "w")
    f.write(s)
    f.close()

    # gen Header
    s = '#ifndef _RPC_META_H\n'
    s += '#define _RPC_META_H\n\n'

    field_type_to_str = {
        0: "int",
        1: "rpcString",
        6: "int8_t",
        7: "int16_t",
        8: "float",
    }

    for cls in class_table:
        if cls["c_imp"] == 0: continue

        s += 'typedef struct %s %s;\n' % (cls["class_name"], cls["class_name"])
        s += 'struct %s {\n' % (cls["class_name"])

        for field in cls["args"]:
            field_type_s = None

            if field["class_index"] != -1:
                child = class_table[field["class_index"]]
                field_type_s = child["class_name"]
            else:
                if not field["field_type"] in field_type_to_str:
                    print("field_type: %d not support" % (field["field_type"]))

                field_type_s = field_type_to_str[field["field_type"]]

            if field["array"] == -1:
                # is array
                field_type_s = "rpcArray"
            s += '    %s %s;\n' % (field_type_s, field["field_name"])

        s += '};\n\n'

    s += '#endif /*_RPC_META_H*/'

    f = open(path.replace(".c", ".h"), "w")
    f.write(s)
    f.close()

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(
"""
usage:
python %s input_file output_file
""" %(__file__)
)
        sys.exit(-1)


    rpc_cfg = sys.argv[1]
    if not os.path.exists(rpc_cfg):
        print("%s not exists" % (rpc_cfg))
        sys.exit(-1)

    rpc_meta_file = sys.argv[2]
    class_table, function_table = read_rpc_cfg(rpc_cfg)
    gen_to_c(class_table, function_table, rpc_meta_file)
