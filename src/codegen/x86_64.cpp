#include "x86_64.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// Register name lookup
// ═══════════════════════════════════════════════════════════════

Reg regFromName(const std::string& name) {
    static const std::unordered_map<std::string, Reg> map = {
        {"rax", Reg::RAX}, {"rcx", Reg::RCX}, {"rdx", Reg::RDX}, {"rbx", Reg::RBX},
        {"rsp", Reg::RSP}, {"rbp", Reg::RBP}, {"rsi", Reg::RSI}, {"rdi", Reg::RDI},
        {"r8",  Reg::R8},  {"r9",  Reg::R9},  {"r10", Reg::R10}, {"r11", Reg::R11},
        {"r12", Reg::R12}, {"r13", Reg::R13}, {"r14", Reg::R14}, {"r15", Reg::R15},
        // 32-bit aliases
        {"eax", Reg::RAX}, {"ecx", Reg::RCX}, {"edx", Reg::RDX}, {"ebx", Reg::RBX},
        {"esp", Reg::RSP}, {"ebp", Reg::RBP}, {"esi", Reg::RSI}, {"edi", Reg::RDI},
        // 8-bit aliases
        {"al",  Reg::RAX}, {"cl",  Reg::RCX}, {"dl",  Reg::RDX}, {"bl",  Reg::RBX},
    };
    auto it = map.find(name);
    return it != map.end() ? it->second : Reg::NONE;
}

// ═══════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════

X86_64::X86_64() {
    memset(regUsed_, 0, sizeof(regUsed_));
    // RSP and RBP are always reserved
    regUsed_[static_cast<int>(Reg::RSP)] = true;
    regUsed_[static_cast<int>(Reg::RBP)] = true;
}

// ═══════════════════════════════════════════════════════════════
// Register allocation (simple linear scan)
// ═══════════════════════════════════════════════════════════════

Reg X86_64::allocReg() {
    // Prefer caller-saved registers first
    static const Reg order[] = {
        Reg::RAX, Reg::RCX, Reg::RDX, Reg::RSI, Reg::RDI,
        Reg::R8, Reg::R9, Reg::R10, Reg::R11,
        Reg::RBX, Reg::R12, Reg::R13, Reg::R14, Reg::R15
    };
    for (Reg r : order) {
        int idx = static_cast<int>(r);
        if (!regUsed_[idx]) {
            regUsed_[idx] = true;
            return r;
        }
    }
    throw std::runtime_error("Register allocation failed: no free registers");
}

void X86_64::freeReg(Reg r) {
    if (r == Reg::RSP || r == Reg::RBP || r == Reg::NONE) return;
    regUsed_[static_cast<int>(r)] = false;
}

// ═══════════════════════════════════════════════════════════════
// String literals
// ═══════════════════════════════════════════════════════════════

size_t X86_64::addStringLiteral(const std::string& str) {
    auto it = stringLiterals_.find(str);
    if (it != stringLiterals_.end()) return it->second;
    size_t offset = rodata_.size();
    for (char c : str) rodata_.push_back(static_cast<uint8_t>(c));
    rodata_.push_back(0); // null terminator
    stringLiterals_[str] = offset;
    return offset;
}

// ═══════════════════════════════════════════════════════════════
// Raw byte emission
// ═══════════════════════════════════════════════════════════════

void X86_64::emit(uint8_t b) { code_.push_back(b); }
void X86_64::emitWord(uint16_t w) { emit(w & 0xFF); emit((w >> 8) & 0xFF); }
void X86_64::emitDword(uint32_t d) {
    emit(d & 0xFF); emit((d >> 8) & 0xFF);
    emit((d >> 16) & 0xFF); emit((d >> 24) & 0xFF);
}
void X86_64::emitQword(uint64_t q) {
    emitDword(static_cast<uint32_t>(q));
    emitDword(static_cast<uint32_t>(q >> 32));
}

// ═══════════════════════════════════════════════════════════════
// x86-64 Encoding Helpers
// ═══════════════════════════════════════════════════════════════

void X86_64::emitREX(bool w, Reg reg, Reg rm) {
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (regExtended(reg)) rex |= 0x04;
    if (regExtended(rm))  rex |= 0x01;
    emit(rex);
}

void X86_64::emitREX_W(Reg reg, Reg rm) {
    emitREX(true, reg, rm);
}

void X86_64::emitModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
    emit((mod << 6) | ((reg & 0x07) << 3) | (rm & 0x07));
}

void X86_64::emitModRM_reg(Reg reg, Reg rm) {
    emitModRM(0x03, regIndex(reg), regIndex(rm));
}

void X86_64::emitModRM_mem(Reg reg, Reg base, int32_t disp) {
    uint8_t regBits = regIndex(reg);
    uint8_t baseBits = regIndex(base);

    if (baseBits == 4) {
        // RSP/R12 needs SIB byte
        if (disp == 0 && baseBits != 5) {
            emitModRM(0x00, regBits, 0x04);
            emit(0x24); // SIB: scale=0, index=RSP(none), base=RSP
        } else if (disp >= -128 && disp <= 127) {
            emitModRM(0x01, regBits, 0x04);
            emit(0x24);
            emit(static_cast<uint8_t>(disp));
        } else {
            emitModRM(0x02, regBits, 0x04);
            emit(0x24);
            emitDword(static_cast<uint32_t>(disp));
        }
    } else if (baseBits == 5 && disp == 0) {
        // RBP/R13 with zero displacement still needs disp8
        emitModRM(0x01, regBits, baseBits);
        emit(0x00);
    } else if (disp == 0) {
        emitModRM(0x00, regBits, baseBits);
    } else if (disp >= -128 && disp <= 127) {
        emitModRM(0x01, regBits, baseBits);
        emit(static_cast<uint8_t>(disp));
    } else {
        emitModRM(0x02, regBits, baseBits);
        emitDword(static_cast<uint32_t>(disp));
    }
}

// ═══════════════════════════════════════════════════════════════
// x86-64 Instructions
// ═══════════════════════════════════════════════════════════════

// MOV r64, r64
void X86_64::emitMovRR(Reg dst, Reg src) {
    if (dst == src) return;
    emitREX_W(src, dst);
    emit(0x89);
    emitModRM_reg(src, dst);
}

// MOV r64, imm64  (full 64-bit immediate)
void X86_64::emitMovRI(Reg dst, int64_t imm) {
    if (imm >= INT32_MIN && imm <= INT32_MAX) {
        // Use MOV r64, imm32 (sign-extended)
        emitREX_W(Reg::RAX, dst);
        emit(0xC7);
        emitModRM(0x03, 0, regIndex(dst));
        emitDword(static_cast<uint32_t>(imm));
    } else {
        // Full 64-bit move: REX.W + B8+rd + imm64
        emitREX_W(Reg::RAX, dst);
        emit(0xB8 + regIndex(dst));
        emitQword(static_cast<uint64_t>(imm));
    }
}

// MOV r32, imm32 (zero-extended to 64-bit)
void X86_64::emitMovRI32(Reg dst, int32_t imm) {
    if (regExtended(dst)) {
        emit(0x41);
    }
    emit(0xB8 + regIndex(dst));
    emitDword(static_cast<uint32_t>(imm));
}

// Load: MOV r64, [base+offset]
void X86_64::emitMovLoad(Reg dst, Reg base, int32_t offset) {
    emitREX_W(dst, base);
    emit(0x8B);
    emitModRM_mem(dst, base, offset);
}

// Store: MOV [base+offset], r64
void X86_64::emitMovStore(Reg base, int32_t offset, Reg src) {
    emitREX_W(src, base);
    emit(0x89);
    emitModRM_mem(src, base, offset);
}

// Store 8-bit: MOV [base+offset], r8
void X86_64::emitMovStore8(Reg base, int32_t offset, Reg src) {
    if (regExtended(src) || regExtended(base)) {
        emitREX(false, src, base);
    }
    emit(0x88);
    emitModRM_mem(src, base, offset);
}

// Store 16-bit
void X86_64::emitMovStore16(Reg base, int32_t offset, Reg src) {
    emit(0x66); // Operand size prefix
    if (regExtended(src) || regExtended(base)) {
        emitREX(false, src, base);
    }
    emit(0x89);
    emitModRM_mem(src, base, offset);
}

// Store 32-bit
void X86_64::emitMovStore32(Reg base, int32_t offset, Reg src) {
    if (regExtended(src) || regExtended(base)) {
        emitREX(false, src, base);
    }
    emit(0x89);
    emitModRM_mem(src, base, offset);
}

// LEA r64, [base+offset]
void X86_64::emitLea(Reg dst, Reg base, int32_t offset) {
    emitREX_W(dst, base);
    emit(0x8D);
    emitModRM_mem(dst, base, offset);
}

// ADD r64, r64
void X86_64::emitAddRR(Reg dst, Reg src) {
    emitREX_W(src, dst);
    emit(0x01);
    emitModRM_reg(src, dst);
}

// ADD r64, imm32
void X86_64::emitAddRI(Reg dst, int32_t imm) {
    emitREX_W(Reg::RAX, dst);
    if (imm >= -128 && imm <= 127) {
        emit(0x83);
        emitModRM(0x03, 0, regIndex(dst));
        emit(static_cast<uint8_t>(imm));
    } else {
        emit(0x81);
        emitModRM(0x03, 0, regIndex(dst));
        emitDword(static_cast<uint32_t>(imm));
    }
}

// SUB r64, r64
void X86_64::emitSubRR(Reg dst, Reg src) {
    emitREX_W(src, dst);
    emit(0x29);
    emitModRM_reg(src, dst);
}

// SUB r64, imm32
void X86_64::emitSubRI(Reg dst, int32_t imm) {
    emitREX_W(Reg::RAX, dst);
    if (imm >= -128 && imm <= 127) {
        emit(0x83);
        emitModRM(0x03, 5, regIndex(dst));
        emit(static_cast<uint8_t>(imm));
    } else {
        emit(0x81);
        emitModRM(0x03, 5, regIndex(dst));
        emitDword(static_cast<uint32_t>(imm));
    }
}

// IMUL r64, r64
void X86_64::emitIMulRR(Reg dst, Reg src) {
    emitREX_W(dst, src);
    emit(0x0F); emit(0xAF);
    emitModRM_reg(dst, src);
}

// IDIV r64 (RDX:RAX / r64 → quotient in RAX, remainder in RDX)
void X86_64::emitIDiv(Reg divisor) {
    emitREX_W(Reg::RAX, divisor);
    emit(0xF7);
    emitModRM(0x03, 7, regIndex(divisor));
}

// CMP r64, r64
void X86_64::emitCmpRR(Reg a, Reg b) {
    emitREX_W(b, a);
    emit(0x39);
    emitModRM_reg(b, a);
}

// CMP r64, imm32
void X86_64::emitCmpRI(Reg a, int32_t imm) {
    emitREX_W(Reg::RAX, a);
    if (imm >= -128 && imm <= 127) {
        emit(0x83);
        emitModRM(0x03, 7, regIndex(a));
        emit(static_cast<uint8_t>(imm));
    } else {
        emit(0x81);
        emitModRM(0x03, 7, regIndex(a));
        emitDword(static_cast<uint32_t>(imm));
    }
}

// TEST r64, r64
void X86_64::emitTestRR(Reg a, Reg b) {
    emitREX_W(b, a);
    emit(0x85);
    emitModRM_reg(b, a);
}

// XOR r64, r64
void X86_64::emitXorRR(Reg dst, Reg src) {
    emitREX_W(src, dst);
    emit(0x31);
    emitModRM_reg(src, dst);
}

// AND r64, r64
void X86_64::emitAndRR(Reg dst, Reg src) {
    emitREX_W(src, dst);
    emit(0x21);
    emitModRM_reg(src, dst);
}

// OR r64, r64
void X86_64::emitOrRR(Reg dst, Reg src) {
    emitREX_W(src, dst);
    emit(0x09);
    emitModRM_reg(src, dst);
}

// NOT r64
void X86_64::emitNotR(Reg dst) {
    emitREX_W(Reg::RAX, dst);
    emit(0xF7);
    emitModRM(0x03, 2, regIndex(dst));
}

// NEG r64
void X86_64::emitNegR(Reg dst) {
    emitREX_W(Reg::RAX, dst);
    emit(0xF7);
    emitModRM(0x03, 3, regIndex(dst));
}

// SHL r64, imm8
void X86_64::emitShlRI(Reg dst, uint8_t count) {
    emitREX_W(Reg::RAX, dst);
    emit(0xC1);
    emitModRM(0x03, 4, regIndex(dst));
    emit(count);
}

// SHR r64, imm8 (logical)
void X86_64::emitShrRI(Reg dst, uint8_t count) {
    emitREX_W(Reg::RAX, dst);
    emit(0xC1);
    emitModRM(0x03, 5, regIndex(dst));
    emit(count);
}

// SAR r64, imm8 (arithmetic)
void X86_64::emitSarRI(Reg dst, uint8_t count) {
    emitREX_W(Reg::RAX, dst);
    emit(0xC1);
    emitModRM(0x03, 7, regIndex(dst));
    emit(count);
}

// PUSH r64
void X86_64::emitPush(Reg r) {
    if (regExtended(r)) emit(0x41);
    emit(0x50 + regIndex(r));
}

// POP r64
void X86_64::emitPop(Reg r) {
    if (regExtended(r)) emit(0x41);
    emit(0x58 + regIndex(r));
}

// CALL label (rel32)
void X86_64::emitCall(const std::string& label) {
    emit(0xE8);
    addPendingJump(codeSize(), label);
    emitDword(0); // placeholder rel32
}

// CALL r64
void X86_64::emitCallReg(Reg r) {
    if (regExtended(r)) emit(0x41);
    emit(0xFF);
    emitModRM(0x03, 2, regIndex(r));
}

// RET
void X86_64::emitRet() { emit(0xC3); }

// JMP label (rel32)
void X86_64::emitJmp(const std::string& label) {
    emit(0xE9);
    addPendingJump(codeSize(), label);
    emitDword(0);
}

// JMP short (rel8)
void X86_64::emitJmpShort(int8_t offset) {
    emit(0xEB);
    emit(static_cast<uint8_t>(offset));
}

// Conditional jumps (rel32)
void X86_64::emitJe(const std::string& label)  { emit(0x0F); emit(0x84); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJne(const std::string& label) { emit(0x0F); emit(0x85); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJl(const std::string& label)  { emit(0x0F); emit(0x8C); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJle(const std::string& label) { emit(0x0F); emit(0x8E); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJg(const std::string& label)  { emit(0x0F); emit(0x8F); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJge(const std::string& label) { emit(0x0F); emit(0x8D); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJa(const std::string& label)  { emit(0x0F); emit(0x87); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJae(const std::string& label) { emit(0x0F); emit(0x83); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJb(const std::string& label)  { emit(0x0F); emit(0x82); addPendingJump(codeSize(), label); emitDword(0); }
void X86_64::emitJbe(const std::string& label) { emit(0x0F); emit(0x86); addPendingJump(codeSize(), label); emitDword(0); }

// SETcc r8 (set byte on condition)
void X86_64::emitSetcc(uint8_t cc, Reg dst) {
    if (regExtended(dst)) {
        emitREX(false, Reg::RAX, dst);
    }
    emit(0x0F);
    emit(0x90 + cc);
    emitModRM(0x03, 0, regIndex(dst));
}

// CDQ (sign-extend EAX into EDX:EAX)
void X86_64::emitCdq() { emit(0x99); }
// CQO (sign-extend RAX into RDX:RAX)
void X86_64::emitCqo() { emit(0x48); emit(0x99); }

// SYSCALL
void X86_64::emitSyscall() { emit(0x0F); emit(0x05); }

// INT n
void X86_64::emitInt(uint8_t vector) { emit(0xCD); emit(vector); }

// IN AL, DX (port I/O)
void X86_64::emitIn8(Reg) { emit(0xEC); }
void X86_64::emitIn16(Reg) { emit(0x66); emit(0xED); }

// OUT DX, AL
void X86_64::emitOut8(Reg) { emit(0xEE); }
void X86_64::emitOut16(Reg) { emit(0x66); emit(0xEF); }

// NOP
void X86_64::emitNop() { emit(0x90); }
// HLT
void X86_64::emitHlt() { emit(0xF4); }
// CLI
void X86_64::emitCli() { emit(0xFA); }
// STI
void X86_64::emitSti() { emit(0xFB); }

// MOV CRn, r64
void X86_64::emitMovCR(int crN, Reg src) {
    emit(0x0F); emit(0x22);
    emitModRM(0x03, crN & 0x07, regIndex(src));
}

// MOV r64, CRn
void X86_64::emitMovFromCR(Reg dst, int crN) {
    emit(0x0F); emit(0x20);
    emitModRM(0x03, crN & 0x07, regIndex(dst));
}

// ═══════════════════════════════════════════════════════════════
// Label management
// ═══════════════════════════════════════════════════════════════

void X86_64::defineLabel(const std::string& name) {
    labels_[name] = codeSize();
}

void X86_64::addPendingJump(size_t patchOffset, const std::string& label) {
    pendingJumps_.push_back({patchOffset, label});
}

void X86_64::resolveJumps() {
    for (auto& pj : pendingJumps_) {
        auto it = labels_.find(pj.label);
        if (it == labels_.end()) {
            throw std::runtime_error("Undefined label: " + pj.label);
        }
        int32_t rel = static_cast<int32_t>(it->second - (pj.patchOffset + 4));
        code_[pj.patchOffset]     = rel & 0xFF;
        code_[pj.patchOffset + 1] = (rel >> 8) & 0xFF;
        code_[pj.patchOffset + 2] = (rel >> 16) & 0xFF;
        code_[pj.patchOffset + 3] = (rel >> 24) & 0xFF;
    }
    pendingJumps_.clear();
}

// Generate JMP [RIP+disp32] thunks for each external function that was called.
// These thunks serve as intermediaries: CALL rel32 -> thunk -> JMP [IAT entry].
// The PE writer patches the disp32 to point at the correct IAT slot.
void X86_64::generateImportThunks() {
    for (const auto& funcName : externalFunctions_) {
        // Only generate a thunk if the function is actually called
        bool called = false;
        for (const auto& pj : pendingJumps_) {
            if (pj.label == funcName) { called = true; break; }
        }
        if (!called) continue;

        // Define a label so CALL rel32 resolves to this thunk
        defineLabel(funcName);

        // JMP [RIP+disp32] = FF 25 xx xx xx xx
        emit(0xFF);
        emit(0x25);
        importRelocs_.push_back({codeSize(), funcName});
        emitDword(0); // placeholder disp32 — PE writer fills this in
    }
}

std::string X86_64::uniqueLabel(const std::string& prefix) {
    return ".L" + prefix + "_" + std::to_string(labelCounter_++);
}

// ═══════════════════════════════════════════════════════════════
// Function frame
// ═══════════════════════════════════════════════════════════════

void X86_64::emitPrologue(int localSize) {
    emitPush(Reg::RBP);
    emitMovRR(Reg::RBP, Reg::RSP);
    if (localSize > 0) {
        // Align to 16 bytes
        localSize = (localSize + 15) & ~15;
        emitSubRI(Reg::RSP, localSize);
    }
}

void X86_64::emitEpilogue() {
    emitMovRR(Reg::RSP, Reg::RBP);
    emitPop(Reg::RBP);
    emitRet();
}

// ═══════════════════════════════════════════════════════════════
// Scope management
// ═══════════════════════════════════════════════════════════════

void X86_64::pushScope() {
    Scope s;
    if (!scopes_.empty()) {
        s.nextOffset = scopes_.back().nextOffset;
    }
    scopes_.push_back(s);
}

void X86_64::popScope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

VarInfo* X86_64::lookupVar(const std::string& name) {
    for (int i = (int)scopes_.size() - 1; i >= 0; i--) {
        auto it = scopes_[i].vars.find(name);
        if (it != scopes_[i].vars.end()) return &it->second;
    }
    return nullptr;
}

VarInfo& X86_64::declareVar(const std::string& name, const TypeInfo& type, bool isParam, int paramIdx) {
    if (scopes_.empty()) pushScope();
    auto& scope = scopes_.back();

    VarInfo v;
    v.name = name;
    v.type = type;
    v.isParam = isParam;

    if (isParam) {
        // Parameters are at positive offsets from RBP (above return address)
        // For now place params in stack frame like locals
        Reg argR = (callingConv_ == CallingConv::WIN64) ? argReg_Win64(paramIdx) : argReg_SysV(paramIdx);
        v.stackOffset = scope.nextOffset;
        scope.nextOffset -= 8;
    } else {
        v.stackOffset = scope.nextOffset;
        scope.nextOffset -= 8;
    }

    scope.vars[name] = v;
    return scope.vars[name];
}

// ═══════════════════════════════════════════════════════════════
// Code Generation — Program & Statements
// ═══════════════════════════════════════════════════════════════

void X86_64::generate(const std::shared_ptr<ASTNode>& ast) {
    genProgram(ast);
    generateImportThunks();
    resolveJumps();
}

void X86_64::genProgram(const std::shared_ptr<ASTNode>& node) {
    pushScope();
    for (auto& child : node->children) {
        genStatement(child);
    }
    popScope();
}

void X86_64::genStatement(const std::shared_ptr<ASTNode>& node) {
    switch (node->type) {
        case NodeType::VAR_DECL:
        case NodeType::CONST_DECL:
        case NodeType::MUT_DECL:
            genVarDecl(node); break;
        case NodeType::FUNC_DECL:
            genFuncDecl(node); break;
        case NodeType::EXTERN_FUNC_DECL:
            // External functions: record symbol and track for import thunks
            symbols_.push_back({node->name, 0, true, -1});
            externalFunctions_.insert(node->name);
            break;
        case NodeType::STRUCT_DECL:
            genStructDecl(node); break;
        case NodeType::RETURN_STMT:
            genReturn(node); break;
        case NodeType::IF_STMT:
            genIf(node); break;
        case NodeType::WHILE_STMT:
            genWhile(node); break;
        case NodeType::FOR_STMT:
            genFor(node); break;
        case NodeType::LOOP_STMT:
            genLoop(node); break;
        case NodeType::BREAK_STMT:
            if (!breakLabels_.empty()) emitJmp(breakLabels_.back());
            break;
        case NodeType::CONTINUE_STMT:
            if (!continueLabels_.empty()) emitJmp(continueLabels_.back());
            break;
        case NodeType::SWITCH_STMT:
            genSwitch(node); break;
        case NodeType::ASSEMBLY_BLOCK:
            genAssemblyBlock(node); break;
        case NodeType::DIRECTIVE_STMT:
            genDirective(node); break;
        case NodeType::LABEL_STMT:
            defineLabel(node->name); break;
        case NodeType::GOTO_STMT:
            emitJmp(node->name); break;
        case NodeType::MODULE_DECL:
            if (node->body) genBlock(node->body);
            break;
        case NodeType::IMPORT_STMT:
            // Handled at a higher level
            break;
        case NodeType::BLOCK:
            genBlock(node); break;
        case NodeType::EXPR_STMT:
            if (node->left) {
                Reg r = genExpr(node->left);
                freeReg(r);
            }
            break;
        default:
            break;
    }
}

void X86_64::genBlock(const std::shared_ptr<ASTNode>& node) {
    pushScope();
    for (auto& child : node->children) {
        genStatement(child);
    }
    popScope();
}

// ═══════════════════════════════════════════════════════════════
// Variable Declaration
// ═══════════════════════════════════════════════════════════════

void X86_64::genVarDecl(const std::shared_ptr<ASTNode>& node) {
    TypeInfo type = node->typeAnnotation ? *node->typeAnnotation : *TypeInfo::makeI64();
    auto& var = declareVar(node->name, type);

    if (node->left) {
        Reg val = genExpr(node->left);
        emitMovStore(Reg::RBP, var.stackOffset, val);
        freeReg(val);
    } else {
        // Zero-initialize
        emitMovRI(Reg::RAX, 0);
        emitMovStore(Reg::RBP, var.stackOffset, Reg::RAX);
    }
}

// ═══════════════════════════════════════════════════════════════
// Function Declaration
// ═══════════════════════════════════════════════════════════════

void X86_64::genFuncDecl(const std::shared_ptr<ASTNode>& node) {
    // Record symbol
    defineLabel(node->name);
    symbols_.push_back({node->name, codeSize(), node->isExport, 0});

    if (node->name == "main" || node->name == "_start") {
        entryPoint_ = codeSize();
    }

    // Prologue: push rbp; mov rbp, rsp; sub rsp, <placeholder>
    emitPush(Reg::RBP);
    emitMovRR(Reg::RBP, Reg::RSP);
    // Emit SUB RSP, imm32 (48 81 EC xx xx xx xx) — patch after body
    emit(0x48); emit(0x81); emit(0xEC);
    size_t prologueSubPos = codeSize();
    emitDword(0); // placeholder

    pushScope();

    // Declare parameters and store incoming register args
    for (int i = 0; i < (int)node->params.size(); i++) {
        auto& p = node->params[i];
        if (p.isVariadic) break;
        TypeInfo pType = p.type ? *p.type : *TypeInfo::makeI64();
        auto& var = declareVar(p.name, pType, true, i);

        Reg argR = (callingConv_ == CallingConv::WIN64) ? argReg_Win64(i) : argReg_SysV(i);
        if (argR != Reg::NONE) {
            emitMovStore(Reg::RBP, var.stackOffset, argR);
        }
    }

    // Track initial scope offset (before locals)
    int scopeStartOffset = scopes_.back().nextOffset;

    // Generate body
    if (node->body) {
        for (auto& child : node->body->children) {
            genStatement(child);
        }
    }

    // Calculate actual local stack space needed
    int scopeEndOffset = scopes_.back().nextOffset;
    int localVarSpace = scopeStartOffset - scopeEndOffset; // positive = bytes used by body locals
    // Add space for params that were stored on stack
    int totalLocalSpace = -(scopeEndOffset + 8); // total bytes below RBP used
    // Align to 16 bytes (and ensure at least 32 for Win64 shadow space in leaf calls)
    totalLocalSpace = (totalLocalSpace + 15) & ~15;
    if (totalLocalSpace == 0) totalLocalSpace = 16; // minimum allocation

    // Patch the prologue SUB RSP immediate
    code_[prologueSubPos + 0] = totalLocalSpace & 0xFF;
    code_[prologueSubPos + 1] = (totalLocalSpace >> 8) & 0xFF;
    code_[prologueSubPos + 2] = (totalLocalSpace >> 16) & 0xFF;
    code_[prologueSubPos + 3] = (totalLocalSpace >> 24) & 0xFF;

    popScope();
    emitEpilogue();
}

// ═══════════════════════════════════════════════════════════════
// Structure Declaration (compile-time only for now)
// ═══════════════════════════════════════════════════════════════

void X86_64::genStructDecl(const std::shared_ptr<ASTNode>&) {
    // Struct layout: record field offsets for later use
    // (Full implementation would maintain a struct table)
}

// ═══════════════════════════════════════════════════════════════
// Return Statement
// ═══════════════════════════════════════════════════════════════

void X86_64::genReturn(const std::shared_ptr<ASTNode>& node) {
    if (node->left) {
        Reg val = genExpr(node->left);
        if (val != Reg::RAX) emitMovRR(Reg::RAX, val);
        freeReg(val);
    }
    emitEpilogue();
}

// ═══════════════════════════════════════════════════════════════
// If Statement
// ═══════════════════════════════════════════════════════════════

void X86_64::genIf(const std::shared_ptr<ASTNode>& node) {
    std::string elseLabel = uniqueLabel("else");
    std::string endLabel = uniqueLabel("endif");

    Reg cond = genExpr(node->condition);
    emitTestRR(cond, cond);
    freeReg(cond);
    emitJe(node->elseBody ? elseLabel : endLabel);

    if (node->body) genBlock(node->body);

    if (node->elseBody) {
        emitJmp(endLabel);
        defineLabel(elseLabel);
        if (node->elseBody->type == NodeType::IF_STMT) {
            genIf(node->elseBody);
        } else {
            genBlock(node->elseBody);
        }
    }

    defineLabel(endLabel);
}

// ═══════════════════════════════════════════════════════════════
// While Loop
// ═══════════════════════════════════════════════════════════════

void X86_64::genWhile(const std::shared_ptr<ASTNode>& node) {
    std::string startLabel = uniqueLabel("while");
    std::string endLabel = uniqueLabel("endwhile");

    breakLabels_.push_back(endLabel);
    continueLabels_.push_back(startLabel);

    defineLabel(startLabel);
    Reg cond = genExpr(node->condition);
    emitTestRR(cond, cond);
    freeReg(cond);
    emitJe(endLabel);

    if (node->body) genBlock(node->body);
    emitJmp(startLabel);
    defineLabel(endLabel);

    breakLabels_.pop_back();
    continueLabels_.pop_back();
}

// ═══════════════════════════════════════════════════════════════
// For Loop
// ═══════════════════════════════════════════════════════════════

void X86_64::genFor(const std::shared_ptr<ASTNode>& node) {
    std::string condLabel = uniqueLabel("for_cond");
    std::string stepLabel = uniqueLabel("for_step");
    std::string endLabel = uniqueLabel("endfor");

    breakLabels_.push_back(endLabel);
    continueLabels_.push_back(stepLabel);

    pushScope();

    // Init
    if (node->init) genStatement(node->init);

    defineLabel(condLabel);

    // Condition
    if (node->condition) {
        Reg cond = genExpr(node->condition);
        emitTestRR(cond, cond);
        freeReg(cond);
        emitJe(endLabel);
    }

    // Body
    if (node->body) genBlock(node->body);

    defineLabel(stepLabel);
    // Step
    if (node->step) {
        Reg val = genExpr(node->step);
        freeReg(val);
    }

    emitJmp(condLabel);
    defineLabel(endLabel);

    popScope();
    breakLabels_.pop_back();
    continueLabels_.pop_back();
}

// ═══════════════════════════════════════════════════════════════
// Infinite Loop
// ═══════════════════════════════════════════════════════════════

void X86_64::genLoop(const std::shared_ptr<ASTNode>& node) {
    std::string startLabel = uniqueLabel("loop");
    std::string endLabel = uniqueLabel("endloop");

    breakLabels_.push_back(endLabel);
    continueLabels_.push_back(startLabel);

    defineLabel(startLabel);
    if (node->body) genBlock(node->body);
    emitJmp(startLabel);
    defineLabel(endLabel);

    breakLabels_.pop_back();
    continueLabels_.pop_back();
}

// ═══════════════════════════════════════════════════════════════
// Switch Statement
// ═══════════════════════════════════════════════════════════════

void X86_64::genSwitch(const std::shared_ptr<ASTNode>& node) {
    std::string endLabel = uniqueLabel("endswitch");
    breakLabels_.push_back(endLabel);

    Reg val = genExpr(node->condition);

    std::vector<std::string> caseLabels;
    std::string defaultLabel = endLabel;

    // Generate comparisons
    for (size_t i = 0; i < node->cases.size(); i++) {
        auto& c = node->cases[i];
        std::string label = uniqueLabel("case");
        caseLabels.push_back(label);

        if (c->left) {
            Reg caseVal = genExpr(c->left);
            emitCmpRR(val, caseVal);
            freeReg(caseVal);
            emitJe(label);
        } else {
            defaultLabel = label;
        }
    }

    freeReg(val);
    emitJmp(defaultLabel);

    // Generate case bodies
    for (size_t i = 0; i < node->cases.size(); i++) {
        defineLabel(caseLabels[i]);
        if (node->cases[i]->body) genBlock(node->cases[i]->body);
    }

    defineLabel(endLabel);
    breakLabels_.pop_back();
}

// ═══════════════════════════════════════════════════════════════
// Assembly Block — translate each AsmInstr to machine code
// ═══════════════════════════════════════════════════════════════

void X86_64::genAssemblyBlock(const std::shared_ptr<ASTNode>& node) {
    for (auto& instr : node->asmInstructions) {
        genAsmInstruction(instr);
    }
}

Reg X86_64::genAsmInstruction(const AsmInstr& instr) {
    std::string mnemonic = instr.mnemonic;
    // Convert to lowercase
    std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);

    if (mnemonic == "nop") {
        emitNop();
    } else if (mnemonic == "ret") {
        emitRet();
    } else if (mnemonic == "syscall") {
        emitSyscall();
    } else if (mnemonic == "hlt") {
        emitHlt();
    } else if (mnemonic == "cli") {
        emitCli();
    } else if (mnemonic == "sti") {
        emitSti();
    } else if (mnemonic == "int") {
        int vec = std::stoi(instr.operand1, nullptr, 0);
        emitInt(static_cast<uint8_t>(vec));
    } else if (mnemonic == "mov") {
        Reg dst = regFromName(instr.operand1);
        Reg src = regFromName(instr.operand2);
        if (dst != Reg::NONE && src != Reg::NONE) {
            emitMovRR(dst, src);
        } else if (dst != Reg::NONE) {
            int64_t imm = std::stoll(instr.operand2, nullptr, 0);
            emitMovRI(dst, imm);
        }
    } else if (mnemonic == "add") {
        Reg dst = regFromName(instr.operand1);
        Reg src = regFromName(instr.operand2);
        if (dst != Reg::NONE && src != Reg::NONE) {
            emitAddRR(dst, src);
        } else if (dst != Reg::NONE) {
            emitAddRI(dst, std::stoi(instr.operand2, nullptr, 0));
        }
    } else if (mnemonic == "sub") {
        Reg dst = regFromName(instr.operand1);
        Reg src = regFromName(instr.operand2);
        if (dst != Reg::NONE && src != Reg::NONE) {
            emitSubRR(dst, src);
        } else if (dst != Reg::NONE) {
            emitSubRI(dst, std::stoi(instr.operand2, nullptr, 0));
        }
    } else if (mnemonic == "xor") {
        Reg dst = regFromName(instr.operand1);
        Reg src = regFromName(instr.operand2);
        if (dst != Reg::NONE && src != Reg::NONE) emitXorRR(dst, src);
    } else if (mnemonic == "and") {
        Reg dst = regFromName(instr.operand1);
        Reg src = regFromName(instr.operand2);
        if (dst != Reg::NONE && src != Reg::NONE) emitAndRR(dst, src);
    } else if (mnemonic == "or") {
        Reg dst = regFromName(instr.operand1);
        Reg src = regFromName(instr.operand2);
        if (dst != Reg::NONE && src != Reg::NONE) emitOrRR(dst, src);
    } else if (mnemonic == "not") {
        Reg dst = regFromName(instr.operand1);
        if (dst != Reg::NONE) emitNotR(dst);
    } else if (mnemonic == "neg") {
        Reg dst = regFromName(instr.operand1);
        if (dst != Reg::NONE) emitNegR(dst);
    } else if (mnemonic == "push") {
        Reg r = regFromName(instr.operand1);
        if (r != Reg::NONE) emitPush(r);
    } else if (mnemonic == "pop") {
        Reg r = regFromName(instr.operand1);
        if (r != Reg::NONE) emitPop(r);
    } else if (mnemonic == "cmp") {
        Reg a = regFromName(instr.operand1);
        Reg b = regFromName(instr.operand2);
        if (a != Reg::NONE && b != Reg::NONE) emitCmpRR(a, b);
        else if (a != Reg::NONE) emitCmpRI(a, std::stoi(instr.operand2, nullptr, 0));
    } else if (mnemonic == "call") {
        Reg r = regFromName(instr.operand1);
        if (r != Reg::NONE) emitCallReg(r);
        else emitCall(instr.operand1);
    } else if (mnemonic == "jmp") {
        emitJmp(instr.operand1);
    } else if (mnemonic == "je" || mnemonic == "jz") {
        emitJe(instr.operand1);
    } else if (mnemonic == "jne" || mnemonic == "jnz") {
        emitJne(instr.operand1);
    } else if (mnemonic == "jl") {
        emitJl(instr.operand1);
    } else if (mnemonic == "jle") {
        emitJle(instr.operand1);
    } else if (mnemonic == "jg") {
        emitJg(instr.operand1);
    } else if (mnemonic == "jge") {
        emitJge(instr.operand1);
    } else if (mnemonic == "shl") {
        Reg dst = regFromName(instr.operand1);
        if (dst != Reg::NONE) emitShlRI(dst, (uint8_t)std::stoi(instr.operand2));
    } else if (mnemonic == "shr") {
        Reg dst = regFromName(instr.operand1);
        if (dst != Reg::NONE) emitShrRI(dst, (uint8_t)std::stoi(instr.operand2));
    } else if (mnemonic == "lea") {
        // Simplified lea: lea dst, [base+offset]
        Reg dst = regFromName(instr.operand1);
        if (dst != Reg::NONE) {
            emitLea(dst, Reg::RBP, 0);
        }
    } else if (mnemonic == "imul") {
        Reg dst = regFromName(instr.operand1);
        Reg src = regFromName(instr.operand2);
        if (dst != Reg::NONE && src != Reg::NONE) emitIMulRR(dst, src);
    } else if (mnemonic == "idiv") {
        Reg r = regFromName(instr.operand1);
        if (r != Reg::NONE) emitIDiv(r);
    } else if (mnemonic == "in") {
        emitIn8(Reg::RDX);
    } else if (mnemonic == "out") {
        emitOut8(Reg::RDX);
    }

    return Reg::NONE;
}

// ═══════════════════════════════════════════════════════════════
// Directives
// ═══════════════════════════════════════════════════════════════

void X86_64::genDirective(const std::shared_ptr<ASTNode>& node) {
    if (node->directive == "bootloader") {
        bootloaderMode_ = true;
    } else if (node->directive == "section") {
        // Section changes handled at link time
    } else if (node->directive == "global") {
        // Mark symbol as global
        if (node->left && node->left->type == NodeType::IDENTIFIER_EXPR) {
            for (auto& sym : symbols_) {
                if (sym.name == node->left->name) sym.isGlobal = true;
            }
        }
    } else if (node->directive == "align") {
        if (node->left && node->left->type == NodeType::INT_LITERAL) {
            size_t alignment = node->left->intValue;
            while (codeSize() % alignment != 0) emitNop();
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Expression Code Generation
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genExpr(const std::shared_ptr<ASTNode>& node) {
    switch (node->type) {
        case NodeType::INT_LITERAL: {
            Reg r = allocReg();
            emitMovRI(r, node->intValue);
            return r;
        }
        case NodeType::FLOAT_LITERAL: {
            // Store float as raw bits in integer register (simplified)
            Reg r = allocReg();
            union { double d; int64_t i; } u;
            u.d = node->floatValue;
            emitMovRI(r, u.i);
            return r;
        }
        case NodeType::STRING_LITERAL: {
            size_t offset = addStringLiteral(node->stringValue);
            Reg r = allocReg();
            // Always use 64-bit movabs for relocatable addresses
            // (emitMovRI would use 32-bit form for small values, breaking relocation offset)
            emitREX_W(Reg::RAX, r);
            emit(0xB8 + regIndex(r));
            emitQword(static_cast<uint64_t>(offset));
            relocations_.push_back({codeSize() - 8, ".rodata", 1, (int)offset});
            return r;
        }
        case NodeType::CHAR_LITERAL: {
            Reg r = allocReg();
            emitMovRI(r, static_cast<int64_t>(node->charValue));
            return r;
        }
        case NodeType::BOOL_LITERAL: {
            Reg r = allocReg();
            emitMovRI(r, node->boolValue ? 1 : 0);
            return r;
        }
        case NodeType::NULL_LITERAL: {
            Reg r = allocReg();
            emitXorRR(r, r);
            return r;
        }
        case NodeType::IDENTIFIER_EXPR: {
            VarInfo* var = lookupVar(node->name);
            if (var) {
                Reg r = allocReg();
                emitMovLoad(r, Reg::RBP, var->stackOffset);
                return r;
            }
            // Could be a function pointer or global
            Reg r = allocReg();
            emitMovRI(r, 0);
            return r;
        }
        case NodeType::BINARY_EXPR:
            return genBinaryExpr(node);
        case NodeType::UNARY_EXPR:
            return genUnaryExpr(node);
        case NodeType::CALL_EXPR:
            return genCallExpr(node);
        case NodeType::SYSCALL_EXPR:
            return genSyscallExpr(node);
        case NodeType::CAST_EXPR:
        case NodeType::BITCAST_EXPR:
            return genCastExpr(node);
        case NodeType::SIZEOF_EXPR:
            return genSizeofExpr(node);
        case NodeType::ADDRESS_OF_EXPR:
            return genAddressOfExpr(node);
        case NodeType::DEREF_EXPR:
            return genDerefExpr(node);
        case NodeType::ASSIGNMENT_EXPR:
            return genAssignment(node);
        case NodeType::COMPOUND_ASSIGNMENT_EXPR:
            return genCompoundAssignment(node);
        case NodeType::MEMBER_EXPR:
            return genMemberExpr(node);
        case NodeType::INDEX_EXPR:
            return genIndexExpr(node);
        case NodeType::MEMORY_EXPR:
            return genMemoryExpr(node);
        case NodeType::PORT_EXPR:
            return genPortExpr(node);
        case NodeType::REGISTER_EXPR:
            return genRegisterExpr(node);
        case NodeType::TERNARY_EXPR: {
            std::string falseLabel = uniqueLabel("tern_false");
            std::string endLabel = uniqueLabel("tern_end");
            Reg cond = genExpr(node->condition);
            emitTestRR(cond, cond);
            freeReg(cond);
            emitJe(falseLabel);
            Reg result = genExpr(node->left);
            emitJmp(endLabel);
            defineLabel(falseLabel);
            Reg alt = genExpr(node->right);
            if (alt != result) emitMovRR(result, alt);
            freeReg(alt);
            defineLabel(endLabel);
            return result;
        }
        default: {
            Reg r = allocReg();
            emitXorRR(r, r);
            return r;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Binary Expressions
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genBinaryExpr(const std::shared_ptr<ASTNode>& node) {
    const std::string& op = node->op;

    // Short-circuit for logical operators
    if (op == "&&") {
        std::string falseLabel = uniqueLabel("and_false");
        std::string endLabel = uniqueLabel("and_end");
        Reg left = genExpr(node->left);
        emitTestRR(left, left);
        emitJe(falseLabel);
        freeReg(left);
        Reg right = genExpr(node->right);
        emitTestRR(right, right);
        emitJe(falseLabel);
        emitMovRI(right, 1);
        emitJmp(endLabel);
        defineLabel(falseLabel);
        emitMovRI(right, 0);
        defineLabel(endLabel);
        return right;
    }
    if (op == "||") {
        std::string trueLabel = uniqueLabel("or_true");
        std::string endLabel = uniqueLabel("or_end");
        Reg left = genExpr(node->left);
        emitTestRR(left, left);
        emitJne(trueLabel);
        freeReg(left);
        Reg right = genExpr(node->right);
        emitTestRR(right, right);
        emitJne(trueLabel);
        emitMovRI(right, 0);
        emitJmp(endLabel);
        defineLabel(trueLabel);
        emitMovRI(right, 1);
        defineLabel(endLabel);
        return right;
    }

    Reg left = genExpr(node->left);
    Reg right = genExpr(node->right);

    if (op == "+") {
        emitAddRR(left, right);
        freeReg(right);
        return left;
    } else if (op == "-") {
        emitSubRR(left, right);
        freeReg(right);
        return left;
    } else if (op == "*") {
        emitIMulRR(left, right);
        freeReg(right);
        return left;
    } else if (op == "/") {
        // IDIV uses RDX:RAX
        emitPush(Reg::RDX);
        if (left != Reg::RAX) {
            emitPush(Reg::RAX);
            emitMovRR(Reg::RAX, left);
        }
        emitCqo();
        emitIDiv(right);
        if (left != Reg::RAX) {
            emitMovRR(left, Reg::RAX);
            emitPop(Reg::RAX);
        }
        emitPop(Reg::RDX);
        freeReg(right);
        return left;
    } else if (op == "%") {
        emitPush(Reg::RDX);
        if (left != Reg::RAX) {
            emitPush(Reg::RAX);
            emitMovRR(Reg::RAX, left);
        }
        emitCqo();
        emitIDiv(right);
        emitMovRR(left, Reg::RDX); // Remainder in RDX
        if (left != Reg::RAX) emitPop(Reg::RAX);
        emitPop(Reg::RDX);
        freeReg(right);
        return left;
    } else if (op == "&") {
        emitAndRR(left, right);
        freeReg(right);
        return left;
    } else if (op == "|") {
        emitOrRR(left, right);
        freeReg(right);
        return left;
    } else if (op == "^") {
        emitXorRR(left, right);
        freeReg(right);
        return left;
    } else if (op == "<<") {
        // SHL: shift count must be in CL
        emitPush(Reg::RCX);
        emitMovRR(Reg::RCX, right);
        emitREX_W(Reg::RAX, left);
        emit(0xD3); emitModRM(0x03, 4, regIndex(left));
        emitPop(Reg::RCX);
        freeReg(right);
        return left;
    } else if (op == ">>") {
        emitPush(Reg::RCX);
        emitMovRR(Reg::RCX, right);
        emitREX_W(Reg::RAX, left);
        emit(0xD3); emitModRM(0x03, 7, regIndex(left));
        emitPop(Reg::RCX);
        freeReg(right);
        return left;
    }

    // Comparison operators => result is 0 or 1
    emitCmpRR(left, right);
    freeReg(right);

    // Zero-extend the result register first
    emitXorRR(left, left);  // Clear entire register

    uint8_t cc = 0;
    if      (op == "==") cc = 0x04; // E
    else if (op == "!=") cc = 0x05; // NE
    else if (op == "<")  cc = 0x0C; // L
    else if (op == "<=") cc = 0x0E; // LE
    else if (op == ">")  cc = 0x0F; // G
    else if (op == ">=") cc = 0x0D; // GE

    emitSetcc(cc, left);
    return left;
}

// ═══════════════════════════════════════════════════════════════
// Unary Expressions
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genUnaryExpr(const std::shared_ptr<ASTNode>& node) {
    Reg operand = genExpr(node->left);
    const std::string& op = node->op;

    if (op == "-") {
        emitNegR(operand);
    } else if (op == "!") {
        emitTestRR(operand, operand);
        emitXorRR(operand, operand);
        emitSetcc(0x04, operand); // SETE
    } else if (op == "~") {
        emitNotR(operand);
    } else if (op == "++" || op == "--") {
        // Pre-increment/decrement
        if (op == "++") emitAddRI(operand, 1);
        else emitSubRI(operand, 1);
        // If operand is a variable, write back
        if (node->left && node->left->type == NodeType::IDENTIFIER_EXPR) {
            VarInfo* var = lookupVar(node->left->name);
            if (var) emitMovStore(Reg::RBP, var->stackOffset, operand);
        }
    }
    return operand;
}

// ═══════════════════════════════════════════════════════════════
// Function Call
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genCallExpr(const std::shared_ptr<ASTNode>& node) {
    // Save caller-saved registers that are in use
    std::vector<Reg> savedRegs;
    static const Reg callerSaved[] = { Reg::RAX, Reg::RCX, Reg::RDX, Reg::RSI, Reg::RDI, Reg::R8, Reg::R9, Reg::R10, Reg::R11 };
    for (Reg r : callerSaved) {
        if (regUsed_[static_cast<int>(r)]) {
            savedRegs.push_back(r);
            emitPush(r);
        }
    }

    int numArgs = (int)node->arguments.size();

    if (callingConv_ == CallingConv::WIN64) {
        // Win64 ABI: allocate shadow space (32 bytes) + stack args in one block
        // Stack args go at [RSP+32], [RSP+40], etc. from callee's perspective
        // But we allocate them BEFORE the call, so from our perspective:
        // [RSP+32] = arg5, [RSP+40] = arg6, etc.
        int stackArgs = numArgs > 4 ? numArgs - 4 : 0;
        // Total stack space: 32 (shadow) + stackArgs*8, aligned to 16
        int stackSpace = 32 + stackArgs * 8;
        // Ensure 16-byte alignment: RSP must be 16-aligned at the CALL instruction.
        // After push rbp + sub rsp (prologue), RSP is 0 mod 16.
        // savedRegs pushes + stackSpace must be 0 mod 16 so RSP stays 0 mod 16 at CALL.
        int preCallPushed = (int)savedRegs.size() * 8 + stackSpace;
        if (preCallPushed % 16 != 0) stackSpace += 8;
        emitSubRI(Reg::RSP, stackSpace);

        // Evaluate all arguments
        // First evaluate stack args (5th, 6th, ...) and store at [RSP+32+i*8]
        for (int i = 4; i < numArgs; i++) {
            Reg val = genExpr(node->arguments[i]);
            emitMovStore(Reg::RSP, 32 + (i - 4) * 8, val);
            freeReg(val);
        }

        // Then evaluate register args (evaluating later avoids clobbering registers)
        for (int i = std::min(numArgs, 4) - 1; i >= 0; i--) {
            Reg val = genExpr(node->arguments[i]);
            Reg argR = argReg_Win64(i);
            if (val != argR) emitMovRR(argR, val);
            freeReg(val);
        }

        // Call
        std::string funcName;
        if (node->left && !node->left->name.empty()) funcName = node->left->name;
        else funcName = node->name;
        emitCall(funcName);

        // Clean up stack space
        emitAddRI(Reg::RSP, stackSpace);
    } else {
        // SysV ABI: evaluate args and place in registers / push to stack
        for (int i = 0; i < numArgs; i++) {
            Reg val = genExpr(node->arguments[i]);
            Reg argR = argReg_SysV(i);
            if (argR != Reg::NONE) {
                if (val != argR) emitMovRR(argR, val);
            } else {
                emitPush(val);
            }
            freeReg(val);
        }

        std::string funcName;
        if (node->left && !node->left->name.empty()) funcName = node->left->name;
        else funcName = node->name;
        emitCall(funcName);
    }

    // Restore caller-saved registers
    for (int i = (int)savedRegs.size() - 1; i >= 0; i--) {
        emitPop(savedRegs[i]);
    }

    // Result is in RAX
    Reg result = allocReg();
    if (result != Reg::RAX) emitMovRR(result, Reg::RAX);
    return result;
}

// ═══════════════════════════════════════════════════════════════
// Syscall Expression
//   syscall(number, arg1, arg2, ...)
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genSyscallExpr(const std::shared_ptr<ASTNode>& node) {
    if (node->arguments.empty()) {
        throw std::runtime_error("syscall requires at least a syscall number");
    }

    // Save registers
    emitPush(Reg::RCX);
    emitPush(Reg::R11);

    // First argument is syscall number -> RAX
    Reg numReg = genExpr(node->arguments[0]);
    if (numReg != Reg::RAX) emitMovRR(Reg::RAX, numReg);
    freeReg(numReg);

    // Remaining arguments go into syscall registers
    for (int i = 1; i < (int)node->arguments.size() && i <= 6; i++) {
        Reg val = genExpr(node->arguments[i]);
        Reg target = syscallArgReg(i - 1);
        if (target != Reg::NONE && val != target) {
            emitMovRR(target, val);
        }
        freeReg(val);
    }

    emitSyscall();

    emitPop(Reg::R11);
    emitPop(Reg::RCX);

    // Result in RAX
    Reg result = allocReg();
    if (result != Reg::RAX) emitMovRR(result, Reg::RAX);
    return result;
}

// ═══════════════════════════════════════════════════════════════
// Cast Expression
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genCastExpr(const std::shared_ptr<ASTNode>& node) {
    // For now, just pass through the value
    // Real implementation would handle sign/zero extension, truncation
    Reg val = genExpr(node->left);

    if (node->castType) {
        size_t srcSize = 8; // Assume 64-bit source
        size_t dstSize = node->castType->sizeBytes();

        if (dstSize < srcSize) {
            // Truncation: mask off extra bits
            if (dstSize == 1) { emitAndRR(val, val); /* movzx would be better */ }
            else if (dstSize == 2) { /* movzx */ }
            else if (dstSize == 4) {
                // Moving to 32-bit register auto-zeros upper 32 bits
            }
        }
        // Sign/zero extension handled elsewhere
    }
    return val;
}

// ═══════════════════════════════════════════════════════════════
// Sizeof Expression
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genSizeofExpr(const std::shared_ptr<ASTNode>& node) {
    Reg r = allocReg();
    size_t size = 0;
    if (node->typeAnnotation) {
        size = node->typeAnnotation->sizeBytes();
    } else {
        size = 8; // Default to pointer size
    }
    emitMovRI(r, static_cast<int64_t>(size));
    return r;
}

// ═══════════════════════════════════════════════════════════════
// Address-of Expression
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genAddressOfExpr(const std::shared_ptr<ASTNode>& node) {
    if (node->left && node->left->type == NodeType::IDENTIFIER_EXPR) {
        VarInfo* var = lookupVar(node->left->name);
        if (var) {
            Reg r = allocReg();
            emitLea(r, Reg::RBP, var->stackOffset);
            return r;
        }
    }
    Reg r = allocReg();
    emitXorRR(r, r);
    return r;
}

// ═══════════════════════════════════════════════════════════════
// Dereference Expression
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genDerefExpr(const std::shared_ptr<ASTNode>& node) {
    Reg ptr = genExpr(node->left);
    Reg result = allocReg();
    emitMovLoad(result, ptr, 0);
    freeReg(ptr);
    return result;
}

// ═══════════════════════════════════════════════════════════════
// Assignment
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genAssignment(const std::shared_ptr<ASTNode>& node) {
    Reg val = genExpr(node->right);

    if (node->left && node->left->type == NodeType::IDENTIFIER_EXPR) {
        VarInfo* var = lookupVar(node->left->name);
        if (var) {
            emitMovStore(Reg::RBP, var->stackOffset, val);
        }
    } else if (node->left && node->left->type == NodeType::DEREF_EXPR) {
        Reg addr = genExpr(node->left->left);
        emitMovStore(addr, 0, val);
        freeReg(addr);
    } else if (node->left && node->left->type == NodeType::INDEX_EXPR) {
        Reg base = genExpr(node->left->object);
        Reg idx = genExpr(node->left->left);
        emitIMulRR(idx, idx); // Scale by element size (simplified: assume 8 bytes)
        emitShlRI(idx, 3);
        emitAddRR(base, idx);
        emitMovStore(base, 0, val);
        freeReg(base);
        freeReg(idx);
    }
    return val;
}

// ═══════════════════════════════════════════════════════════════
// Compound Assignment (+=, -=, etc.)
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genCompoundAssignment(const std::shared_ptr<ASTNode>& node) {
    if (node->left && node->left->type == NodeType::IDENTIFIER_EXPR) {
        VarInfo* var = lookupVar(node->left->name);
        if (var) {
            Reg current = allocReg();
            emitMovLoad(current, Reg::RBP, var->stackOffset);
            Reg rhs = genExpr(node->right);

            if (node->op == "+=") emitAddRR(current, rhs);
            else if (node->op == "-=") emitSubRR(current, rhs);
            else if (node->op == "*=") emitIMulRR(current, rhs);
            else if (node->op == "&=") emitAndRR(current, rhs);
            else if (node->op == "|=") emitOrRR(current, rhs);
            else if (node->op == "^=") emitXorRR(current, rhs);

            freeReg(rhs);
            emitMovStore(Reg::RBP, var->stackOffset, current);
            return current;
        }
    }

    Reg r = allocReg();
    emitXorRR(r, r);
    return r;
}

// ═══════════════════════════════════════════════════════════════
// Member Expression
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genMemberExpr(const std::shared_ptr<ASTNode>& node) {
    Reg obj = genExpr(node->object);
    // Simplified: just return the object pointer
    // Full implementation would add field offset
    return obj;
}

// ═══════════════════════════════════════════════════════════════
// Index Expression
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genIndexExpr(const std::shared_ptr<ASTNode>& node) {
    Reg base = genExpr(node->object);
    Reg idx = genExpr(node->left);
    // Assume 8-byte elements
    emitShlRI(idx, 3);
    emitAddRR(base, idx);
    freeReg(idx);
    Reg result = allocReg();
    emitMovLoad(result, base, 0);
    freeReg(base);
    return result;
}

// ═══════════════════════════════════════════════════════════════
// Memory Expressions
//   memory.allocate(size), memory.free(ptr), memory.read_u8(ptr, offset)
//   memory.write_u8(ptr, offset, val), memory.copy(dst, src, len)
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genMemoryExpr(const std::shared_ptr<ASTNode>& node) {
    if (node->name == "read_u8" || node->name == "read_byte") {
        // memory.read_u8(ptr, offset) -> load byte at ptr+offset
        if (node->arguments.size() >= 2) {
            Reg base = genExpr(node->arguments[0]);
            Reg offset = genExpr(node->arguments[1]);
            emitAddRR(base, offset);
            freeReg(offset);
            // MOVZX r64, byte [base]
            emitREX_W(base, base);
            emit(0x0F); emit(0xB6);
            emitModRM(0x00, regIndex(base), regIndex(base));
            return base;
        }
    } else if (node->name == "write_u8" || node->name == "write_byte") {
        if (node->arguments.size() >= 3) {
            Reg base = genExpr(node->arguments[0]);
            Reg offset = genExpr(node->arguments[1]);
            Reg val = genExpr(node->arguments[2]);
            emitAddRR(base, offset);
            emitMovStore8(base, 0, val);
            freeReg(base);
            freeReg(offset);
            return val;
        }
    } else if (node->name == "read_u32") {
        if (node->arguments.size() >= 2) {
            Reg base = genExpr(node->arguments[0]);
            Reg offset = genExpr(node->arguments[1]);
            emitAddRR(base, offset);
            freeReg(offset);
            // MOV r32, [base]
            if (regExtended(base)) emit(0x45); else emit(0x40 | 0x00);
            emit(0x8B);
            emitModRM(0x00, regIndex(base), regIndex(base));
            return base;
        }
    } else if (node->name == "write_u32") {
        if (node->arguments.size() >= 3) {
            Reg base = genExpr(node->arguments[0]);
            Reg offset = genExpr(node->arguments[1]);
            Reg val = genExpr(node->arguments[2]);
            emitAddRR(base, offset);
            emitMovStore32(base, 0, val);
            freeReg(base);
            freeReg(offset);
            return val;
        }
    }

    // For allocate/free — these would be syscalls (mmap/munmap on Linux)
    Reg r = allocReg();
    emitXorRR(r, r);
    return r;
}

// ═══════════════════════════════════════════════════════════════
// Port I/O Expressions
//   port.input_byte(port) -> IN AL, port
//   port.output_byte(port, value) -> OUT port, AL
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genPortExpr(const std::shared_ptr<ASTNode>& node) {
    if (node->name == "input_byte" || node->name == "in8") {
        if (node->arguments.size() >= 1) {
            Reg port = genExpr(node->arguments[0]);
            // Port number must be in DX
            if (port != Reg::RDX) emitMovRR(Reg::RDX, port);
            freeReg(port);
            // IN AL, DX
            emitIn8(Reg::RDX);
            Reg result = allocReg();
            // MOVZX result, AL
            if (result != Reg::RAX) {
                emit(0x48 | (regExtended(result) ? 0x04 : 0));
                emit(0x0F); emit(0xB6);
                emitModRM(0x03, regIndex(result), 0); // AL
            }
            return result;
        }
    } else if (node->name == "output_byte" || node->name == "out8") {
        if (node->arguments.size() >= 2) {
            Reg port = genExpr(node->arguments[0]);
            Reg val = genExpr(node->arguments[1]);
            if (port != Reg::RDX) emitMovRR(Reg::RDX, port);
            if (val != Reg::RAX) emitMovRR(Reg::RAX, val);
            freeReg(port);
            freeReg(val);
            emitOut8(Reg::RDX);
            Reg r = allocReg();
            emitXorRR(r, r);
            return r;
        }
    } else if (node->name == "input_word" || node->name == "in16") {
        if (node->arguments.size() >= 1) {
            Reg port = genExpr(node->arguments[0]);
            if (port != Reg::RDX) emitMovRR(Reg::RDX, port);
            freeReg(port);
            emitIn16(Reg::RDX);
            Reg result = allocReg();
            if (result != Reg::RAX) emitMovRR(result, Reg::RAX);
            return result;
        }
    } else if (node->name == "output_word" || node->name == "out16") {
        if (node->arguments.size() >= 2) {
            Reg port = genExpr(node->arguments[0]);
            Reg val = genExpr(node->arguments[1]);
            if (port != Reg::RDX) emitMovRR(Reg::RDX, port);
            if (val != Reg::RAX) emitMovRR(Reg::RAX, val);
            freeReg(port);
            freeReg(val);
            emitOut16(Reg::RDX);
            Reg r = allocReg();
            emitXorRR(r, r);
            return r;
        }
    }

    Reg r = allocReg();
    emitXorRR(r, r);
    return r;
}

// ═══════════════════════════════════════════════════════════════
// Register Access Expressions
//   register.rax -> read RAX
// ═══════════════════════════════════════════════════════════════

Reg X86_64::genRegisterExpr(const std::shared_ptr<ASTNode>& node) {
    Reg r = regFromName(node->name);
    if (r != Reg::NONE) {
        Reg result = allocReg();
        emitMovRR(result, r);
        return result;
    }
    Reg result = allocReg();
    emitXorRR(result, result);
    return result;
}

// ═══════════════════════════════════════════════════════════════
// Debug: dump generated code as hex
// ═══════════════════════════════════════════════════════════════

void X86_64::dumpCode() const {
    std::cout << "=== Generated x86-64 Code (" << code_.size() << " bytes) ===" << std::endl;
    for (size_t i = 0; i < code_.size(); i++) {
        if (i % 16 == 0) {
            if (i > 0) std::cout << std::endl;
            std::cout << std::hex << std::setw(8) << std::setfill('0') << i << ": ";
        }
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)code_[i] << " ";
    }
    std::cout << std::dec << std::endl;

    std::cout << "\n=== Symbols ===" << std::endl;
    for (auto& sym : symbols_) {
        std::cout << "  " << sym.name << " @ 0x" << std::hex << sym.offset
                  << (sym.isGlobal ? " [global]" : "") << std::dec << std::endl;
    }

    std::cout << "\n=== Labels ===" << std::endl;
    for (auto& [name, offset] : labels_) {
        std::cout << "  " << name << " @ 0x" << std::hex << offset << std::dec << std::endl;
    }
}

} // namespace verslang
