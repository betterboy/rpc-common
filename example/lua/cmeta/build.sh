gcc -O2 -I /usr/include/ -I ../../../src/ -I ./ -L /usr/lib64/ -Wall -fPIC -shared -o RpcMeta.so -llua rpc_meta.c rpc_lua_cmeta.c ../../c/rpc_c_implement.c