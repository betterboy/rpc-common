#include "../src/mbuf.h"
#include <iostream>
#include <string>
#include <string.h>

using namespace std;
using namespace crpc;
int main()
{
    Mbuf buff;
    cout << "buff size= " << buff.size() << endl;
    string data("abcdefg");
    buff.append(data);
    string dequeue(buff.pullup(), buff.size());
    cout << "dequeue= " << dequeue << "buff size=" << buff.size() << endl;
    buff.retrieve(dequeue.size());
    cout << "after retrieve. buff size=" << buff.size() << endl;

    char arr[] = "hijklm";
    cout << "arr len=" << strlen(arr) << endl;
    buff.append(arr, strlen(arr));
    cout << "buff size=" << buff.size() << endl;
    buff.retrieve(strlen(arr));
    cout << "after buff size=" << buff.size() << endl;


    for (int i=0; i <= 100; ++i) {
        buff.append(string(1025, 'b'));
    }
    cout << "buff size= " << buff.size() << endl;
    cout << "buff block size= " << buff.blockCount() << endl;
    char* ret = new char[10000];
    buff.dequeue(ret, 10000);
    buff.retrieve(10000);
    cout << "buff size= " << buff.size() << endl;
    cout << "buff block size= " << buff.blockCount() << endl;

    Mbuf buff2 = buff;
    cout << "buff2 size= " << buff2.size() << endl;
    cout << "buff2 block size= " << buff2.blockCount() << endl;
    buff2.dequeue(ret, 10000);
    buff2.retrieve(10000);
    cout << "buff2 size= " << buff2.size() << endl;
    cout << "buff2 block size= " << buff2.blockCount() << endl;
    return 0;
}
