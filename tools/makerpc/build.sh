
# test_decl/存放协议描述文件
# output/是生成rpc.cfg的目录
lua trans_rpc_cfg.lua examples_decl/ output/

# 将rpc.cfg生成rpc_meta.c, rpc_meta.h
if [[ $? -eq 0 ]]
then
python gen_rpc_meta_c.py output/rpc.cfg output/rpc_meta.c
fi
