#include <windows.h>
int main() {
    int sm = GetSystemMetrics(0);
    ExitProcess((UINT)sm);
    return 0;
}
