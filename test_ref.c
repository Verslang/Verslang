#include <windows.h>
int main() {
    int w = GetSystemMetrics(0);
    ExitProcess((UINT)w);
    return 0;
}
