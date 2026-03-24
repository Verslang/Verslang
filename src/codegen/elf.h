#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// ELF64 Format Writer
// Generates valid ELF64 executables for Linux x86-64
// ═══════════════════════════════════════════════════════════════

class ELF64Writer {
public:
    // Create an ELF64 executable from code + data sections
    static std::vector<uint8_t> createExecutable(
        const std::vector<uint8_t>& code,
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& rodata,
        uint64_t entryOffset = 0
    ) {
        // Memory layout:
        // 0x400000: ELF header + program headers
        // 0x401000: .text (code)
        // 0x402000: .rodata
        // 0x403000: .data

        const uint64_t BASE_ADDR    = 0x400000;
        const uint64_t PAGE_SIZE    = 0x1000;
        const uint64_t ELF_HDR_SIZE = 64;
        const uint64_t PHDR_SIZE    = 56;
        const int NUM_PHDRS         = 3; // LOAD for code, rodata, data
        const uint64_t HDRS_SIZE    = ELF_HDR_SIZE + PHDR_SIZE * NUM_PHDRS;
        const uint64_t TEXT_OFFSET  = PAGE_SIZE; // File offset of .text
        const uint64_t TEXT_VADDR   = BASE_ADDR + TEXT_OFFSET;

        uint64_t rodataFileOff = TEXT_OFFSET + ((code.size() + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
        uint64_t rodataVaddr   = BASE_ADDR + rodataFileOff;
        uint64_t dataFileOff   = rodataFileOff + ((rodata.size() + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
        uint64_t dataVaddr     = BASE_ADDR + dataFileOff;

        uint64_t entryPoint = TEXT_VADDR + entryOffset;

        std::vector<uint8_t> out;
        auto write8  = [&](uint8_t v)  { out.push_back(v); };
        auto write16 = [&](uint16_t v) { out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF); };
        auto write32 = [&](uint32_t v) {
            for (int i = 0; i < 4; i++) { out.push_back((v >> (i * 8)) & 0xFF); }
        };
        auto write64 = [&](uint64_t v) {
            for (int i = 0; i < 8; i++) { out.push_back((v >> (i * 8)) & 0xFF); }
        };
        auto pad = [&](size_t target) {
            while (out.size() < target) out.push_back(0);
        };

        // ── ELF Header (64 bytes) ──
        // e_ident (16 bytes)
        write8(0x7F); write8('E'); write8('L'); write8('F');
        write8(2);        // EI_CLASS: ELFCLASS64
        write8(1);        // EI_DATA: ELFDATA2LSB
        write8(1);        // EI_VERSION: EV_CURRENT
        write8(0);        // EI_OSABI: ELFOSABI_NONE (System V)
        write64(0);       // EI_ABIVERSION + padding
        write16(2);       // e_type: ET_EXEC
        write16(0x3E);    // e_machine: EM_X86_64
        write32(1);       // e_version: EV_CURRENT
        write64(entryPoint); // e_entry
        write64(ELF_HDR_SIZE); // e_phoff (program headers right after ELF header)
        write64(0);       // e_shoff (no section headers for minimal exec)
        write32(0);       // e_flags
        write16(64);      // e_ehsize
        write16(56);      // e_phentsize
        write16(NUM_PHDRS); // e_phnum
        write16(64);      // e_shentsize
        write16(0);       // e_shnum
        write16(0);       // e_shstrndx

        // ── Program Headers ──

        // PT_LOAD: Code segment (RX)
        write32(1);       // p_type: PT_LOAD
        write32(5);       // p_flags: PF_R | PF_X
        write64(TEXT_OFFSET);  // p_offset
        write64(TEXT_VADDR);   // p_vaddr
        write64(TEXT_VADDR);   // p_paddr
        write64(code.size());  // p_filesz
        write64(code.size());  // p_memsz
        write64(PAGE_SIZE);    // p_align

        // PT_LOAD: Rodata segment (R)
        write32(1);       // p_type: PT_LOAD
        write32(4);       // p_flags: PF_R
        write64(rodataFileOff); // p_offset
        write64(rodataVaddr);   // p_vaddr
        write64(rodataVaddr);   // p_paddr
        write64(rodata.size()); // p_filesz
        write64(rodata.size()); // p_memsz
        write64(PAGE_SIZE);     // p_align

        // PT_LOAD: Data segment (RW)
        write32(1);       // p_type: PT_LOAD
        write32(6);       // p_flags: PF_R | PF_W
        write64(dataFileOff);  // p_offset
        write64(dataVaddr);    // p_vaddr
        write64(dataVaddr);    // p_paddr
        write64(data.size());  // p_filesz
        write64(data.size());  // p_memsz
        write64(PAGE_SIZE);    // p_align

        // ── Sections ──
        // Pad to .text
        pad(TEXT_OFFSET);
        // Write code
        out.insert(out.end(), code.begin(), code.end());

        // Pad to .rodata
        pad(rodataFileOff);
        out.insert(out.end(), rodata.begin(), rodata.end());

        // Pad to .data
        pad(dataFileOff);
        out.insert(out.end(), data.begin(), data.end());

        return out;
    }

    // Create a minimal static executable (no section headers, just loads)
    static std::vector<uint8_t> createMinimalExec(
        const std::vector<uint8_t>& code,
        uint64_t entryOffset = 0
    ) {
        const uint64_t BASE_ADDR = 0x400000;
        const uint64_t HDR_SIZE  = 64 + 56; // ELF header + 1 program header = 120 bytes
        const uint64_t TEXT_VADDR = BASE_ADDR + HDR_SIZE;

        std::vector<uint8_t> out;
        auto write8  = [&](uint8_t v)  { out.push_back(v); };
        auto write16 = [&](uint16_t v) { out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF); };
        auto write32 = [&](uint32_t v) {
            for (int i = 0; i < 4; i++) { out.push_back((v >> (i * 8)) & 0xFF); }
        };
        auto write64 = [&](uint64_t v) {
            for (int i = 0; i < 8; i++) { out.push_back((v >> (i * 8)) & 0xFF); }
        };

        uint64_t fileSize = HDR_SIZE + code.size();

        // ELF Header
        write8(0x7F); write8('E'); write8('L'); write8('F');
        write8(2); write8(1); write8(1); write8(0);
        write64(0);
        write16(2);       // ET_EXEC
        write16(0x3E);    // EM_X86_64
        write32(1);
        write64(TEXT_VADDR + entryOffset);
        write64(64);      // e_phoff
        write64(0);       // e_shoff
        write32(0);
        write16(64);
        write16(56);
        write16(1);       // 1 program header
        write16(64);
        write16(0);
        write16(0);

        // Single PT_LOAD header (entire file mapped RWX)
        write32(1);       // PT_LOAD
        write32(7);       // PF_R | PF_W | PF_X
        write64(0);       // p_offset: 0 (map from start)
        write64(BASE_ADDR);
        write64(BASE_ADDR);
        write64(fileSize);
        write64(fileSize);
        write64(0x200000); // p_align

        // Code
        out.insert(out.end(), code.begin(), code.end());

        return out;
    }
};

} // namespace verslang
