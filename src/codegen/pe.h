#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include "x86_64.h"

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// PE32+ (64-bit) Format Writer
// Generates valid Windows x64 executables (.exe)
// ═══════════════════════════════════════════════════════════════

class PE64Writer {
public:
    // Look up the DLL that exports a given Win32 function
    static std::string getDllForFunction(const std::string& funcName) {
        static const std::unordered_map<std::string, std::string> table = {
            // ── kernel32.dll ──
            {"ExitProcess", "kernel32.dll"},
            {"GetStdHandle", "kernel32.dll"},
            {"WriteConsoleA", "kernel32.dll"},
            {"WriteConsoleW", "kernel32.dll"},
            {"ReadConsoleA", "kernel32.dll"},
            {"GetModuleHandleA", "kernel32.dll"},
            {"GetModuleHandleW", "kernel32.dll"},
            {"GetModuleFileNameA", "kernel32.dll"},
            {"GetModuleFileNameW", "kernel32.dll"},
            {"GetProcAddress", "kernel32.dll"},
            {"LoadLibraryA", "kernel32.dll"},
            {"LoadLibraryW", "kernel32.dll"},
            {"FreeLibrary", "kernel32.dll"},
            {"CreateDirectoryA", "kernel32.dll"},
            {"CreateDirectoryW", "kernel32.dll"},
            {"RemoveDirectoryA", "kernel32.dll"},
            {"CopyFileA", "kernel32.dll"},
            {"CopyFileW", "kernel32.dll"},
            {"DeleteFileA", "kernel32.dll"},
            {"MoveFileA", "kernel32.dll"},
            {"GetFileAttributesA", "kernel32.dll"},
            {"GetFileAttributesW", "kernel32.dll"},
            {"CreateFileA", "kernel32.dll"},
            {"CreateFileW", "kernel32.dll"},
            {"ReadFile", "kernel32.dll"},
            {"WriteFile", "kernel32.dll"},
            {"CloseHandle", "kernel32.dll"},
            {"Sleep", "kernel32.dll"},
            {"GetLastError", "kernel32.dll"},
            {"SetLastError", "kernel32.dll"},
            {"GetCommandLineA", "kernel32.dll"},
            {"GetCommandLineW", "kernel32.dll"},
            {"GetEnvironmentVariableA", "kernel32.dll"},
            {"SetEnvironmentVariableA", "kernel32.dll"},
            {"GetCurrentDirectoryA", "kernel32.dll"},
            {"SetCurrentDirectoryA", "kernel32.dll"},
            {"GetTickCount", "kernel32.dll"},
            {"QueryPerformanceCounter", "kernel32.dll"},
            {"QueryPerformanceFrequency", "kernel32.dll"},
            {"VirtualAlloc", "kernel32.dll"},
            {"VirtualFree", "kernel32.dll"},
            {"HeapAlloc", "kernel32.dll"},
            {"HeapFree", "kernel32.dll"},
            {"GetProcessHeap", "kernel32.dll"},
            {"GlobalAlloc", "kernel32.dll"},
            {"GlobalFree", "kernel32.dll"},
            {"LocalAlloc", "kernel32.dll"},
            {"LocalFree", "kernel32.dll"},
            {"MultiByteToWideChar", "kernel32.dll"},
            {"WideCharToMultiByte", "kernel32.dll"},
            {"GetSystemDirectoryA", "kernel32.dll"},
            {"GetWindowsDirectoryA", "kernel32.dll"},
            {"GetTempPathA", "kernel32.dll"},
            {"FindFirstFileA", "kernel32.dll"},
            {"FindNextFileA", "kernel32.dll"},
            {"FindClose", "kernel32.dll"},
            // ── user32.dll ──
            {"RegisterClassExA", "user32.dll"},
            {"RegisterClassExW", "user32.dll"},
            {"RegisterClassA", "user32.dll"},
            {"UnregisterClassA", "user32.dll"},
            {"CreateWindowExA", "user32.dll"},
            {"CreateWindowExW", "user32.dll"},
            {"ShowWindow", "user32.dll"},
            {"UpdateWindow", "user32.dll"},
            {"DestroyWindow", "user32.dll"},
            {"PostQuitMessage", "user32.dll"},
            {"DefWindowProcA", "user32.dll"},
            {"DefWindowProcW", "user32.dll"},
            {"InvalidateRect", "user32.dll"},
            {"SetTimer", "user32.dll"},
            {"KillTimer", "user32.dll"},
            {"GetMessageA", "user32.dll"},
            {"GetMessageW", "user32.dll"},
            {"PeekMessageA", "user32.dll"},
            {"TranslateMessage", "user32.dll"},
            {"DispatchMessageA", "user32.dll"},
            {"DispatchMessageW", "user32.dll"},
            {"SendMessageA", "user32.dll"},
            {"SendMessageW", "user32.dll"},
            {"PostMessageA", "user32.dll"},
            {"GetDC", "user32.dll"},
            {"ReleaseDC", "user32.dll"},
            {"BeginPaint", "user32.dll"},
            {"EndPaint", "user32.dll"},
            {"FillRect", "user32.dll"},
            {"DrawTextA", "user32.dll"},
            {"LoadCursorA", "user32.dll"},
            {"LoadCursorW", "user32.dll"},
            {"LoadIconA", "user32.dll"},
            {"LoadIconW", "user32.dll"},
            {"GetSystemMetrics", "user32.dll"},
            {"SetWindowPos", "user32.dll"},
            {"GetClientRect", "user32.dll"},
            {"GetWindowRect", "user32.dll"},
            {"MoveWindow", "user32.dll"},
            {"SetWindowTextA", "user32.dll"},
            {"GetWindowTextA", "user32.dll"},
            {"MessageBoxA", "user32.dll"},
            {"MessageBoxW", "user32.dll"},
            {"SetFocus", "user32.dll"},
            {"GetFocus", "user32.dll"},
            {"EnableWindow", "user32.dll"},
            {"IsWindow", "user32.dll"},
            {"GetParent", "user32.dll"},
            {"SetParent", "user32.dll"},
            {"GetDlgItem", "user32.dll"},
            {"SetCursor", "user32.dll"},
            {"GetCursorPos", "user32.dll"},
            {"ScreenToClient", "user32.dll"},
            {"ClientToScreen", "user32.dll"},
            {"RedrawWindow", "user32.dll"},
            {"GetDesktopWindow", "user32.dll"},
            {"SetCapture", "user32.dll"},
            {"ReleaseCapture", "user32.dll"},
            {"TrackMouseEvent", "user32.dll"},
            {"GetKeyState", "user32.dll"},
            {"GetAsyncKeyState", "user32.dll"},
            {"SetRect", "user32.dll"},
            {"InflateRect", "user32.dll"},
            {"PtInRect", "user32.dll"},
            {"AdjustWindowRectEx", "user32.dll"},
            {"SystemParametersInfoA", "user32.dll"},
            // ── gdi32.dll ──
            {"CreateSolidBrush", "gdi32.dll"},
            {"CreatePen", "gdi32.dll"},
            {"SelectObject", "gdi32.dll"},
            {"DeleteObject", "gdi32.dll"},
            {"SetBkMode", "gdi32.dll"},
            {"SetTextColor", "gdi32.dll"},
            {"SetBkColor", "gdi32.dll"},
            {"TextOutA", "gdi32.dll"},
            {"TextOutW", "gdi32.dll"},
            {"ExtTextOutA", "gdi32.dll"},
            {"Rectangle", "gdi32.dll"},
            {"Ellipse", "gdi32.dll"},
            {"MoveToEx", "gdi32.dll"},
            {"LineTo", "gdi32.dll"},
            {"CreateFontA", "gdi32.dll"},
            {"CreateFontW", "gdi32.dll"},
            {"CreateFontIndirectA", "gdi32.dll"},
            {"RoundRect", "gdi32.dll"},
            {"CreateCompatibleDC", "gdi32.dll"},
            {"CreateCompatibleBitmap", "gdi32.dll"},
            {"BitBlt", "gdi32.dll"},
            {"StretchBlt", "gdi32.dll"},
            {"DeleteDC", "gdi32.dll"},
            {"GetDeviceCaps", "gdi32.dll"},
            {"GetTextExtentPoint32A", "gdi32.dll"},
            {"GetStockObject", "gdi32.dll"},
            {"SetPixel", "gdi32.dll"},
            {"GetPixel", "gdi32.dll"},
            {"CreateDIBSection", "gdi32.dll"},
            {"SetDIBitsToDevice", "gdi32.dll"},
            {"PatBlt", "gdi32.dll"},
            {"Polygon", "gdi32.dll"},
            {"Polyline", "gdi32.dll"},
            {"Arc", "gdi32.dll"},
            {"Pie", "gdi32.dll"},
            {"SetMapMode", "gdi32.dll"},
            {"SaveDC", "gdi32.dll"},
            {"RestoreDC", "gdi32.dll"},
            // ── dwmapi.dll ──
            {"DwmSetWindowAttribute", "dwmapi.dll"},
            {"DwmGetWindowAttribute", "dwmapi.dll"},
            {"DwmExtendFrameIntoClientArea", "dwmapi.dll"},
            {"DwmIsCompositionEnabled", "dwmapi.dll"},
            // ── shell32.dll ──
            {"ShellExecuteA", "shell32.dll"},
            {"ShellExecuteW", "shell32.dll"},
            {"SHGetFolderPathA", "shell32.dll"},
            {"SHGetFolderPathW", "shell32.dll"},
            {"SHBrowseForFolderA", "shell32.dll"},
            {"SHGetPathFromIDListA", "shell32.dll"},
            // ── advapi32.dll ──
            {"RegSetKeyValueA", "advapi32.dll"},
            {"RegSetKeyValueW", "advapi32.dll"},
            {"RegOpenKeyExA", "advapi32.dll"},
            {"RegCloseKey", "advapi32.dll"},
            {"RegQueryValueExA", "advapi32.dll"},
            {"RegCreateKeyExA", "advapi32.dll"},
            {"RegDeleteKeyA", "advapi32.dll"},
            {"RegDeleteValueA", "advapi32.dll"},
            // ── comctl32.dll ──
            {"InitCommonControlsEx", "comctl32.dll"},
            {"InitCommonControls", "comctl32.dll"},
            // ── msvcrt.dll (C runtime) ──
            {"malloc", "msvcrt.dll"},
            {"free", "msvcrt.dll"},
            {"memcpy", "msvcrt.dll"},
            {"memset", "msvcrt.dll"},
            {"strlen", "msvcrt.dll"},
            {"strcmp", "msvcrt.dll"},
            {"strcpy", "msvcrt.dll"},
            {"strcat", "msvcrt.dll"},
            {"printf", "msvcrt.dll"},
            {"sprintf", "msvcrt.dll"},
            {"sscanf", "msvcrt.dll"},
            {"atoi", "msvcrt.dll"},
            {"exit", "msvcrt.dll"},
            // ── ws2_32.dll (Winsock) ──
            {"WSAStartup", "ws2_32.dll"},
            {"WSACleanup", "ws2_32.dll"},
            {"socket", "ws2_32.dll"},
            {"bind", "ws2_32.dll"},
            {"listen", "ws2_32.dll"},
            {"accept", "ws2_32.dll"},
            {"connect", "ws2_32.dll"},
            {"send", "ws2_32.dll"},
            {"recv", "ws2_32.dll"},
            {"closesocket", "ws2_32.dll"},
        };
        auto it = table.find(funcName);
        if (it != table.end()) return it->second;
        return "kernel32.dll"; // default fallback
    }

private:
    static uint32_t alignUp(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    struct SectionInfo {
        const char* name;
        const std::vector<uint8_t>* data;
        uint32_t chars;
        uint32_t va;
        uint32_t rawSize;
        uint32_t fileOff;
    };

    // Build a minimal .rsrc section containing VS_VERSION_INFO
    // The Data Entry's OffsetToData (at byte offset 0x48) is a placeholder
    // that must be patched to rsrcVA + 0x58 after section layout is known.
    static std::vector<uint8_t> buildVersionResourceData() {
        std::vector<uint8_t> r;
        auto w16 = [&](uint16_t v) { r.push_back(v&0xFF); r.push_back((v>>8)&0xFF); };
        auto w32 = [&](uint32_t v) { for(int i=0;i<4;i++) r.push_back((v>>(i*8))&0xFF); };

        // ── Resource directory tree (3 levels) ──
        // Root Directory (16 bytes @ 0x00)
        w32(0); w32(0); w16(0); w16(0); w16(0); w16(1);
        // Root Entry: RT_VERSION=16 → Level2 @ 0x18 (8 bytes @ 0x10)
        w32(16); w32(0x80000018);
        // Level 2 Directory (16 bytes @ 0x18)
        w32(0); w32(0); w16(0); w16(0); w16(0); w16(1);
        // Level 2 Entry: ID=1 → Level3 @ 0x30 (8 bytes @ 0x28)
        w32(1); w32(0x80000030);
        // Level 3 Directory (16 bytes @ 0x30)
        w32(0); w32(0); w16(0); w16(0); w16(0); w16(1);
        // Level 3 Entry: Lang=0x0409 → DataEntry @ 0x48 (8 bytes @ 0x40)
        w32(0x0409); w32(0x48);

        // ── Resource Data Entry (16 bytes @ 0x48) ──
        w32(0);    // OffsetToData placeholder → patched to rsrcVA + 0x58
        w32(92);   // Size of VS_VERSIONINFO
        w32(0);    // CodePage
        w32(0);    // Reserved

        // ── VS_VERSIONINFO (92 bytes @ 0x58) ──
        w16(92);   // wLength
        w16(52);   // wValueLength (sizeof VS_FIXEDFILEINFO)
        w16(0);    // wType (binary)
        // szKey: L"VS_VERSION_INFO\0" (16 wchars = 32 bytes)
        const char* key = "VS_VERSION_INFO";
        for (int i = 0; key[i]; i++) w16((uint16_t)(unsigned char)key[i]);
        w16(0);
        // Padding to DWORD boundary (6+32=38 → 40)
        w16(0);
        // VS_FIXEDFILEINFO (52 bytes)
        w32(0xFEEF04BD);  // signature
        w32(0x00010000);  // struct version
        w32(0x00010000);  // file version MS (1.0)
        w32(0x00000000);  // file version LS
        w32(0x00010000);  // product version MS
        w32(0x00000000);  // product version LS
        w32(0x0000003F);  // flags mask
        w32(0x00000000);  // flags
        w32(0x00040004);  // OS (VOS_NT_WINDOWS32)
        w32(0x00000001);  // file type (VFT_APP)
        w32(0x00000000);  // subtype
        w32(0x00000000);  // date MS
        w32(0x00000000);  // date LS

        return r;  // 180 bytes
    }

    // Patch the Resource Data Entry's OffsetToData field after section VA is known
    static void patchResourceRVA(std::vector<uint8_t>& rsrc, uint32_t rsrcVA) {
        uint32_t dataRVA = rsrcVA + 0x58;
        rsrc[0x48] = dataRVA & 0xFF;
        rsrc[0x49] = (dataRVA >> 8) & 0xFF;
        rsrc[0x4A] = (dataRVA >> 16) & 0xFF;
        rsrc[0x4B] = (dataRVA >> 24) & 0xFF;
    }

public:
    static std::vector<uint8_t> createExecutable(
        const std::vector<uint8_t>& codeIn,
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& rodata,
        uint64_t entryOffset,
        const std::vector<ImportRelocation>& importRelocs = {},
        const std::vector<Relocation>& relocations = {},
        uint16_t subsystem = 3  // 3=CUI, 2=GUI
    ) {
        const uint32_t IMAGE_BASE        = 0x00400000;
        const uint32_t SECTION_ALIGNMENT = 0x1000;
        const uint32_t FILE_ALIGNMENT    = 0x200;

        // Mutable copy of code (we'll patch thunk displacements)
        auto code = codeIn;

        // ── Build import data if we have external function calls ──
        std::vector<uint8_t> idata;
        uint32_t iatRVAInIdata = 0;
        uint32_t iatTotalSize = 0;
        uint32_t idtSize = 0;
        std::unordered_map<std::string, uint32_t> funcToIATOffset;

        if (!importRelocs.empty()) {
            // Group functions by DLL
            std::map<std::string, std::vector<std::string>> dllFuncs;
            for (const auto& ir : importRelocs) {
                std::string dll = getDllForFunction(ir.funcName);
                auto& funcs = dllFuncs[dll];
                if (std::find(funcs.begin(), funcs.end(), ir.funcName) == funcs.end()) {
                    funcs.push_back(ir.funcName);
                }
            }

            int numDlls = (int)dllFuncs.size();

            // Layout within .idata:
            // [IDT] [ILT entries per DLL] [IAT entries per DLL] [Hint/Name table] [DLL names]
            idtSize = (numDlls + 1) * 20;

            uint32_t iltOffset = idtSize;
            uint32_t iltSize = 0;
            std::map<std::string, uint32_t> dllILTOffset;
            for (auto& [dll, funcs] : dllFuncs) {
                dllILTOffset[dll] = iltOffset + iltSize;
                iltSize += ((uint32_t)funcs.size() + 1) * 8;
            }

            uint32_t iatOffset = iltOffset + iltSize;
            iatRVAInIdata = iatOffset;
            uint32_t iatCalc = 0;
            std::map<std::string, uint32_t> dllIATOffset;
            for (auto& [dll, funcs] : dllFuncs) {
                dllIATOffset[dll] = iatOffset + iatCalc;
                for (int i = 0; i < (int)funcs.size(); i++)
                    funcToIATOffset[funcs[i]] = iatOffset + iatCalc + i * 8;
                iatCalc += ((uint32_t)funcs.size() + 1) * 8;
            }
            iatTotalSize = iatCalc;

            uint32_t hnCursor = iatOffset + iatCalc;
            std::unordered_map<std::string, uint32_t> funcHNOffset;
            for (auto& [dll, funcs] : dllFuncs) {
                for (auto& fn : funcs) {
                    funcHNOffset[fn] = hnCursor;
                    uint32_t sz = 2 + (uint32_t)fn.size() + 1;
                    if (sz & 1) sz++;
                    hnCursor += sz;
                }
            }
            std::map<std::string, uint32_t> dllNameOff;
            for (auto& [dll, funcs] : dllFuncs) {
                dllNameOff[dll] = hnCursor;
                hnCursor += (uint32_t)dll.size() + 1;
            }

            idata.resize(hnCursor, 0);

            auto wLE32 = [&](uint32_t off, uint32_t v) {
                idata[off]=v&0xFF; idata[off+1]=(v>>8)&0xFF;
                idata[off+2]=(v>>16)&0xFF; idata[off+3]=(v>>24)&0xFF;
            };
            auto wLE64 = [&](uint32_t off, uint64_t v) {
                for(int i=0;i<8;i++) idata[off+i]=(v>>(i*8))&0xFF;
            };

            // Write Hint/Name entries
            for (auto& [dll, funcs] : dllFuncs)
                for (auto& fn : funcs)
                    memcpy(&idata[funcHNOffset[fn]+2], fn.c_str(), fn.size());

            // Write DLL name strings
            for (auto& [dll, funcs] : dllFuncs)
                memcpy(&idata[dllNameOff[dll]], dll.c_str(), dll.size());

            // We need idataVA to fill in RVAs. Build sections list to compute it.
            // (We'll fill IDT/ILT/IAT after computing section layout below)

            // ── Build section list ──
            std::vector<SectionInfo> sections;
            auto rsrc = buildVersionResourceData();
            sections.push_back({".text",  &code,   0x60000020, 0,0,0});
            if (!rodata.empty())
                sections.push_back({".rdata", &rodata, 0x40000040, 0,0,0});
            sections.push_back({".idata", &idata,  0xC0000040, 0,0,0});
            sections.push_back({".rsrc",  &rsrc,   0x40000040, 0,0,0});
            if (!data.empty())
                sections.push_back({".data",  &data,   0xC0000040, 0,0,0});

            int numSections = (int)sections.size();
            // Header: DOS(128) + PE_sig(4) + COFF(20) + OptHdr(240) + Sections(N*40)
            uint32_t headerBytes = 128 + 4 + 20 + 240 + numSections * 40;
            uint32_t HEADER_SIZE = alignUp(headerBytes, FILE_ALIGNMENT);

            // Compute VAs and file offsets
            uint32_t nextVA = SECTION_ALIGNMENT;
            uint32_t nextFileOff = HEADER_SIZE;
            for (auto& s : sections) {
                s.va = nextVA;
                s.rawSize = alignUp((uint32_t)s.data->size(), FILE_ALIGNMENT);
                s.fileOff = nextFileOff;
                nextVA += alignUp(std::max((uint32_t)s.data->size(), 1u), SECTION_ALIGNMENT);
                nextFileOff += s.rawSize;
            }
            uint32_t imageSize = nextVA;

            // Find the .idata VA
            uint32_t idataVA = 0;
            for (auto& s : sections) { if (std::string(s.name) == ".idata") { idataVA = s.va; break; } }

            // Patch .rsrc data entry with correct RVA
            uint32_t rsrcVA = 0;
            for (auto& s : sections) { if (std::string(s.name) == ".rsrc") { rsrcVA = s.va; break; } }
            if (rsrcVA) patchResourceRVA(rsrc, rsrcVA);

            // Now fill IDT with proper RVAs
            int dllIdx = 0;
            for (auto& [dll, funcs] : dllFuncs) {
                uint32_t e = dllIdx * 20;
                wLE32(e+0,  idataVA + dllILTOffset[dll]);
                wLE32(e+4,  0);
                wLE32(e+8,  0);
                wLE32(e+12, idataVA + dllNameOff[dll]);
                wLE32(e+16, idataVA + dllIATOffset[dll]);
                dllIdx++;
            }
            // Fill ILT
            for (auto& [dll, funcs] : dllFuncs) {
                uint32_t base = dllILTOffset[dll];
                for (int i = 0; i < (int)funcs.size(); i++)
                    wLE64(base + i*8, idataVA + funcHNOffset[funcs[i]]);
            }
            // Fill IAT (same as ILT; loader overwrites at load time)
            for (auto& [dll, funcs] : dllFuncs) {
                uint32_t base = dllIATOffset[dll];
                for (int i = 0; i < (int)funcs.size(); i++)
                    wLE64(base + i*8, idataVA + funcHNOffset[funcs[i]]);
            }

            // Patch code thunks with correct displacements
            uint32_t textVA = sections[0].va;
            for (const auto& ir : importRelocs) {
                auto itIAT = funcToIATOffset.find(ir.funcName);
                if (itIAT == funcToIATOffset.end()) continue;
                int32_t disp = (int32_t)(idataVA + itIAT->second) - (int32_t)(textVA + ir.patchOffset + 4);
                code[ir.patchOffset+0] = disp & 0xFF;
                code[ir.patchOffset+1] = (disp >> 8) & 0xFF;
                code[ir.patchOffset+2] = (disp >> 16) & 0xFF;
                code[ir.patchOffset+3] = (disp >> 24) & 0xFF;
            }

            // Apply section relocations (ABS64 fixups for rodata/data references)
            applyRelocations(code, sections, relocations, IMAGE_BASE);

            return writePE(sections, IMAGE_BASE, SECTION_ALIGNMENT, FILE_ALIGNMENT,
                           HEADER_SIZE, imageSize, entryOffset, subsystem,
                           idataVA, idtSize,
                           idataVA + iatRVAInIdata, iatTotalSize,
                           rsrcVA, (uint32_t)rsrc.size());
        }

        // ── No imports ──
        std::vector<SectionInfo> sections;
        auto rsrc = buildVersionResourceData();
        sections.push_back({".text", &code, 0x60000020, 0,0,0});
        if (!rodata.empty())
            sections.push_back({".rdata", &rodata, 0x40000040, 0,0,0});
        sections.push_back({".rsrc", &rsrc, 0x40000040, 0,0,0});
        if (!data.empty())
            sections.push_back({".data", &data, 0xC0000040, 0,0,0});

        int numSections = (int)sections.size();
        uint32_t headerBytes = 128 + 4 + 20 + 240 + numSections * 40;
        uint32_t HEADER_SIZE = alignUp(headerBytes, FILE_ALIGNMENT);

        uint32_t nextVA = SECTION_ALIGNMENT;
        uint32_t nextFileOff = HEADER_SIZE;
        for (auto& s : sections) {
            s.va = nextVA;
            s.rawSize = alignUp((uint32_t)s.data->size(), FILE_ALIGNMENT);
            s.fileOff = nextFileOff;
            nextVA += alignUp(std::max((uint32_t)s.data->size(), 1u), SECTION_ALIGNMENT);
            nextFileOff += s.rawSize;
        }
        uint32_t imageSize = nextVA;

        // Patch .rsrc data entry with correct RVA
        uint32_t rsrcVA = 0;
        for (auto& s : sections) { if (std::string(s.name) == ".rsrc") { rsrcVA = s.va; break; } }
        if (rsrcVA) patchResourceRVA(rsrc, rsrcVA);

        // Apply section relocations (ABS64 fixups for rodata/data references)
        applyRelocations(code, sections, relocations, IMAGE_BASE);

        return writePE(sections, IMAGE_BASE, SECTION_ALIGNMENT, FILE_ALIGNMENT,
                       HEADER_SIZE, imageSize, entryOffset, subsystem, 0,0, 0,0,
                       rsrcVA, (uint32_t)rsrc.size());
    }

private:

    // Apply ABS64 relocations: patch code with absolute virtual addresses
    // for .rodata/.data section references
    static void applyRelocations(
        std::vector<uint8_t>& code,
        const std::vector<SectionInfo>& sections,
        const std::vector<Relocation>& relocations,
        uint32_t IMAGE_BASE
    ) {
        // Map section names to their VAs (".rodata" in codegen maps to ".rdata" in PE)
        std::unordered_map<std::string, uint32_t> sectionVA;
        for (auto& s : sections) {
            sectionVA[s.name] = s.va;
            // Also map ".rodata" -> same VA as ".rdata"
            if (std::string(s.name) == ".rdata")
                sectionVA[".rodata"] = s.va;
        }

        for (const auto& rel : relocations) {
            if (rel.type != 1) continue; // Only ABS64
            auto it = sectionVA.find(rel.symbol);
            if (it == sectionVA.end()) continue;
            uint64_t absAddr = (uint64_t)IMAGE_BASE + it->second + rel.addend;
            // Patch the 8-byte immediate in code
            if (rel.offset + 8 <= code.size()) {
                code[rel.offset+0] = (absAddr)       & 0xFF;
                code[rel.offset+1] = (absAddr >> 8)  & 0xFF;
                code[rel.offset+2] = (absAddr >> 16) & 0xFF;
                code[rel.offset+3] = (absAddr >> 24) & 0xFF;
                code[rel.offset+4] = (absAddr >> 32) & 0xFF;
                code[rel.offset+5] = (absAddr >> 40) & 0xFF;
                code[rel.offset+6] = (absAddr >> 48) & 0xFF;
                code[rel.offset+7] = (absAddr >> 56) & 0xFF;
            }
        }
    }

    static std::vector<uint8_t> writePE(
        const std::vector<SectionInfo>& sections,
        uint32_t IMAGE_BASE, uint32_t SECTION_ALIGNMENT, uint32_t FILE_ALIGNMENT,
        uint32_t HEADER_SIZE, uint32_t imageSize,
        uint64_t entryOffset, uint16_t subsystem,
        uint32_t importDirRVA, uint32_t importDirSize,
        uint32_t iatRVA, uint32_t iatSize,
        uint32_t rsrcRVA = 0, uint32_t rsrcSize = 0
    ) {
        int numSections = (int)sections.size();
        uint32_t textVA = sections[0].va;

        // Compute SizeOfCode and SizeOfInitializedData
        uint32_t sizeOfCode = 0, sizeOfInitData = 0;
        for (auto& s : sections) {
            if (s.chars & 0x00000020) sizeOfCode += s.rawSize;      // CODE
            if (s.chars & 0x00000040) sizeOfInitData += s.rawSize;  // INITIALIZED_DATA
        }

        std::vector<uint8_t> out;
        auto write8  = [&](uint8_t v)  { out.push_back(v); };
        auto write16 = [&](uint16_t v) { out.push_back(v&0xFF); out.push_back((v>>8)&0xFF); };
        auto write32 = [&](uint32_t v) { for(int i=0;i<4;i++) out.push_back((v>>(i*8))&0xFF); };
        auto write64 = [&](uint64_t v) { for(int i=0;i<8;i++) out.push_back((v>>(i*8))&0xFF); };
        auto writeStr = [&](const char* s, size_t len) {
            for(size_t i=0;i<len;i++) out.push_back(i<strlen(s)?s[i]:0);
        };
        auto pad = [&](size_t target) { while(out.size()<target) out.push_back(0); };

        // DOS Header
        write16(0x5A4D);
        for(int i=0;i<29;i++) write16(0);
        write32(0x80);
        pad(0x80);

        // PE Signature
        write32(0x00004550);

        // COFF Header
        write16(0x8664);
        write16(numSections);
        write32(0); write32(0); write32(0);
        write16(240);
        write16(0x0022);

        // Optional Header (PE32+)
        write16(0x020B);
        write8(14); write8(0);
        write32(sizeOfCode);
        write32(sizeOfInitData);
        write32(0);
        write32(textVA + (uint32_t)entryOffset);
        write32(textVA);
        write64(IMAGE_BASE);
        write32(SECTION_ALIGNMENT);
        write32(FILE_ALIGNMENT);
        write16(6); write16(0);
        write16(0); write16(0);
        write16(6); write16(0);
        write32(0);
        write32(imageSize);
        write32(HEADER_SIZE);
        write32(0);
        write16(subsystem);
        write16(0x8100);  // NX_COMPAT | TERMINAL_SERVER_AWARE (no DYNAMIC_BASE — no .reloc section)
        write64(0x100000);
        write64(0x1000);
        write64(0x100000);
        write64(0x1000);
        write32(0);
        write32(16);

        // Data Directories (16 x 8 bytes)
        write32(0); write32(0);                          // 0: Export
        write32(importDirRVA); write32(importDirSize);   // 1: Import
        write32(rsrcRVA); write32(rsrcSize);             // 2: Resource
        for(int i=3;i<12;i++){write32(0);write32(0);}   // 3-11
        write32(iatRVA); write32(iatSize);               // 12: IAT
        for(int i=13;i<16;i++){write32(0);write32(0);}  // 13-15

        // Section Headers
        for(auto& s : sections) {
            writeStr(s.name, 8);
            write32((uint32_t)s.data->size());
            write32(s.va);
            write32(s.rawSize);
            write32(s.fileOff);
            write32(0); write32(0); write16(0); write16(0);
            write32(s.chars);
        }

        pad(HEADER_SIZE);

        // Section data
        for(auto& s : sections) {
            out.insert(out.end(), s.data->begin(), s.data->end());
            pad(s.fileOff + s.rawSize);
        }

        return out;
    }
};

// ═══════════════════════════════════════════════════════════════
// Raw Binary Writer (for bootloaders)
// ═══════════════════════════════════════════════════════════════

class BinaryWriter {
public:
    // Create a raw binary (e.g., 512-byte boot sector with 0xAA55 signature)
    static std::vector<uint8_t> createBootSector(const std::vector<uint8_t>& code) {
        std::vector<uint8_t> out(512, 0);

        // Copy code (max 510 bytes for boot sector)
        size_t copySize = std::min(code.size(), (size_t)510);
        std::memcpy(out.data(), code.data(), copySize);

        // Boot signature at bytes 510-511
        out[510] = 0x55;
        out[511] = 0xAA;

        return out;
    }

    // Create a flat binary from raw code
    static std::vector<uint8_t> createFlatBinary(
        const std::vector<uint8_t>& code,
        const std::vector<uint8_t>& data = {},
        size_t alignment = 16
    ) {
        std::vector<uint8_t> out = code;

        // Align data section
        while (out.size() % alignment != 0) out.push_back(0);

        out.insert(out.end(), data.begin(), data.end());
        return out;
    }
};

} // namespace verslang
