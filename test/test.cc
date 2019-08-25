#include <vector>
#include <iostream>
using namespace std;

typedef void (*rpc_func_t) (int vfd, int num, ...);

void func(int vfd, int a, int b, int c)
{
    cout << a << " " << b << " " << c << endl;
}

int main()
{
}

