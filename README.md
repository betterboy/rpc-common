# rpc-common
>这是一个用于在线游戏rpc通信的协议模块
该模块只负责解析协议定义文件，协议打包和解包，网络传输和脚本层函数回调需要自己处理，可以查阅其他资料。

## BackGround

在线游戏中，客户端与服务端进行协议通信，通常需要定义好协议参数和回调函数，比如类似于google protobuf 的proto文件，而游戏引擎负责解析协议文件，生成协议定义。游戏开发人员负责定义协议和调用，举个例子:
```lua
    --客户端调用服务器协议函数
    function c2s_set_user_name(uid, name)
        --设置玩家名字
        local user = get_user(uid)
        user:SetName(name)
        --通知客户端
        s2c_user_name(uid, name)
    end
```

当客户端调用c2s_函数时，rpc模块获取函数对应的协议ID，获取参数列表，然后将脚本层传入的参数，打包成二进制，通过网络发送到服务端。服务端收到网络数据，将二进制数据解析成协议ID和参数列表，并传到脚本层，找到该函数，调用，rpc通讯结束。

## Code Structure

--[src]协议源文件，一级目录下为公用代码文件，子目录如lua、python为脚本binding。
--[example]各个绑定的示例代码。
>运行build.sh生成.so文件，然后拷贝到示例代码目录。

## Install
安装该模块非常简单，进入到需要的binding目录，运行build.sh文件，生成.so文件，并在你的代码中载入该库
>chmod +x build.sh
>./build.sh

## Usage
具体用法请查看example目录

## Maintainers
Email: betterboy3@163.com
目前还缺少python接口，后续会添加.
欢迎交流，有任何问题请联系.