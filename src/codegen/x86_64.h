#pragma once
#include "../parser/ast.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <functional>

namespace verslang {

// ═══════════════════════════════════════════════════════════════
// x86-64 Register Encoding
// ═══════════════════════════════════════════════════════════════

enum class Reg : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8  = 8, R9  = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    NONE = 255
};

inline uint8_t regIndex(Reg r) { return static_cast<uint8_t>(r) & 0x07; }
inline bool    regExtended(Reg r) { return static_cast<uint8_t>(r) >= 8; }
inline const char* regName(Reg r) {
    static const char* names[] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8","r9","r10","r11","r12","r13","r14","r15"
    };
    uint8_t i = static_cast<uint8_t>(r);
    return i < 16 ? names[i] : "???";
}

Reg regFromName(const std::string& name);

// ═══════════════════════════════════════════════════════════════
// Calling Conventions
// ═══════════════════════════════════════════════════════════════

enum class CallingConv { SYSV_AMD64, WIN64 };

// SysV AMD64: RDI, RSI, RDX, RCX, R8, R9
// Win64:      RCX, RDX, R8, R9
inline Reg argReg_SysV(int idx) {
    static const Reg regs[] = { Reg::RDI, Reg::RSI, Reg::RDX, Reg::RCX, Reg::R8, Reg::R9 };
    return idx < 6 ? regs[idx] : Reg::NONE;
}
inline Reg argReg_Win64(int idx) {
    static const Reg regs[] = { Reg::RCX, Reg::RDX, Reg::R8, Reg::R9 };
    return idx < 4 ? regs[idx] : Reg::NONE;
}

// Linux syscall convention: RDI, RSI, RDX, R10, R8, R9 (number in RAX)
inline Reg syscallArgReg(int idx) {
    static const Reg regs[] = { Reg::RDI, Reg::RSI, Reg::RDX, Reg::R10, Reg::R8, Reg::R9 };
    return idx < 6 ? regs[idx] : Reg::NONE;
}

// ═══════════════════════════════════════════════════════════════
// Relocation & Symbol types
// ═══════════════════════════════════════════════════════════════

struct Symbol {
    std::string name;
    size_t offset = 0;
    bool isGlobal = false;
    int section = 0;     // 0=code, 1=data, 2=rodata
};

struct Relocation {
    size_t offset;       // Where in code to patch
    std::string symbol;  // Target label/symbol
    int type;            // 0=REL32 (for jmp/call), 1=ABS64 (for data)
    int addend = 0;
};

struct PendingJump {
    size_t patchOffset;  // Where to write the rel32
    std::string label;   // Target label
};

// Import relocation: tracks JMP [RIP+disp32] thunks that need
// patching by the PE writer to point at the correct IAT entry
struct ImportRelocation {
    size_t patchOffset;   // Where in code to write the disp32
    std::string funcName; // Name of the imported function
};

// ═══════════════════════════════════════════════════════════════
// Variable info for stack frame
// ═══════════════════════════════════════════════════════════════

struct VarInfo {
    std::string name;
    TypeInfo type;
    int stackOffset;     // Offset from RBP (negative for locals)
    bool isParam;
};

// ═══════════════════════════════════════════════════════════════
// x86-64 Code Generator
// ═══════════════════════════════════════════════════════════════

class X86_64 {
public:
    X86_64();

    // Set target platform
    void setCallingConv(CallingConv cc) { callingConv_ = cc; }
    void setBootloaderMode(bool v) { bootloaderMode_ = v; }

    // Generate code from AST
    void generate(const std::shared_ptr<ASTNode>& ast);

    // Output
    const std::vector<uint8_t>& code() const { return code_; }
    const std::vector<uint8_t>& data() const { return data_; }
    const std::vector<uint8_t>& rodata() const { return rodata_; }
    const std::vector<Symbol>& symbols() const { return symbols_; }
    const std::vector<Relocation>& relocations() const { return relocations_; }
    const std::vector<ImportRelocation>& importRelocations() const { return importRelocs_; }
    const std::unordered_set<std::string>& externalFunctions() const { return externalFunctions_; }
    size_t entryPoint() const { return entryPoint_; }

    // Debug
    void dumpCode() const;

private:
    std::vector<uint8_t> code_;
    std::vector<uint8_t> data_;
    std::vector<uint8_t> rodata_;
    std::vector<Symbol> symbols_;
    std::vector<Relocation> relocations_;
    std::vector<PendingJump> pendingJumps_;
    std::unordered_map<std::string, size_t> labels_;
    std::unordered_set<std::string> externalFunctions_;  // External function names
    std::vector<ImportRelocation> importRelocs_;          // Thunk fixups for PE writer
    CallingConv callingConv_ = CallingConv::WIN64;
    bool bootloaderMode_ = false;
    size_t entryPoint_ = 0;

    // Scope / variable tracking
    struct Scope {
        std::unordered_map<std::string, VarInfo> vars;
        int nextOffset = -8; // First local at [rbp-8]
    };
    std::vector<Scope> scopes_;
    int currentStackSize_ = 0;
    int maxStackSize_ = 0;

    // Register allocation
    bool regUsed_[16] = {};
    Reg allocReg();
    void freeReg(Reg r);

    // String literal tracking
    std::unordered_map<std::string, size_t> stringLiterals_;
    size_t addStringLiteral(const std::string& str);

    // ─────────────────────────────────────────────
    // Raw byte emission
    // ─────────────────────────────────────────────
    void emit(uint8_t b);
    void emitWord(uint16_t w);
    void emitDword(uint32_t d);
    void emitQword(uint64_t q);
    size_t codeSize() const { return code_.size(); }

    // ─────────────────────────────────────────────
    // x86-64 instruction encoding helpers
    // ─────────────────────────────────────────────
    void emitREX(bool w, Reg reg, Reg rm);
    void emitREX_W(Reg reg, Reg rm);
    void emitModRM(uint8_t mod, uint8_t reg, uint8_t rm);
    void emitModRM_reg(Reg reg, Reg rm);
    void emitModRM_mem(Reg reg, Reg base, int32_t disp);

    // ─────────────────────────────────────────────
    // x86-64 instructions
    // ─────────────────────────────────────────────
    void emitMovRR(Reg dst, Reg src);
    void emitMovRI(Reg dst, int64_t imm);
    void emitMovRI32(Reg dst, int32_t imm);
    void emitMovLoad(Reg dst, Reg base, int32_t offset);    // Load [base+offset] -> dst
    void emitMovStore(Reg base, int32_t offset, Reg src);   // Store src -> [base+offset]
    void emitMovStore8(Reg base, int32_t offset, Reg src);  // Store 8-bit
    void emitMovStore16(Reg base, int32_t offset, Reg src); // Store 16-bit
    void emitMovStore32(Reg base, int32_t offset, Reg src); // Store 32-bit
    void emitLea(Reg dst, Reg base, int32_t offset);
    void emitAddRR(Reg dst, Reg src);
    void emitAddRI(Reg dst, int32_t imm);
    void emitSubRR(Reg dst, Reg src);
    void emitSubRI(Reg dst, int32_t imm);
    void emitIMulRR(Reg dst, Reg src);
    void emitIDiv(Reg divisor);
    void emitCmpRR(Reg a, Reg b);
    void emitCmpRI(Reg a, int32_t imm);
    void emitTestRR(Reg a, Reg b);
    void emitXorRR(Reg dst, Reg src);
    void emitAndRR(Reg dst, Reg src);
    void emitOrRR(Reg dst, Reg src);
    void emitNotR(Reg dst);
    void emitNegR(Reg dst);
    void emitShlRI(Reg dst, uint8_t count);
    void emitShrRI(Reg dst, uint8_t count);
    void emitSarRI(Reg dst, uint8_t count);
    void emitPush(Reg r);
    void emitPop(Reg r);
    void emitCall(const std::string& label);
    void emitCallReg(Reg r);
    void emitRet();
    void emitJmp(const std::string& label);
    void emitJmpShort(int8_t offset);
    void emitJe(const std::string& label);
    void emitJne(const std::string& label);
    void emitJl(const std::string& label);
    void emitJle(const std::string& label);
    void emitJg(const std::string& label);
    void emitJge(const std::string& label);
    void emitJa(const std::string& label);
    void emitJae(const std::string& label);
    void emitJb(const std::string& label);
    void emitJbe(const std::string& label);
    void emitSetcc(uint8_t cc, Reg dst);
    void emitCdq();
    void emitCqo();
    void emitSyscall();
    void emitInt(uint8_t vector);
    void emitIn8(Reg port);     // IN AL, DX
    void emitIn16(Reg port);    // IN AX, DX
    void emitOut8(Reg port);    // OUT DX, AL
    void emitOut16(Reg port);   // OUT DX, AX
    void emitNop();
    void emitHlt();
    void emitCli();
    void emitSti();
    void emitMovCR(int crN, Reg src);  // MOV CRn, reg
    void emitMovFromCR(Reg dst, int crN);

    // ─────────────────────────────────────────────
    // Label management
    // ─────────────────────────────────────────────
    void defineLabel(const std::string& name);
    void addPendingJump(size_t patchOffset, const std::string& label);
    void resolveJumps();
    void generateImportThunks(); // Create JMP [RIP+disp32] thunks for external functions

    // ─────────────────────────────────────────────
    // Function frame helpers
    // ─────────────────────────────────────────────
    void emitPrologue(int localSize);
    void emitEpilogue();

    // ─────────────────────────────────────────────
    // Scope / variable helpers
    // ─────────────────────────────────────────────
    void pushScope();
    void popScope();
    VarInfo* lookupVar(const std::string& name);
    VarInfo& declareVar(const std::string& name, const TypeInfo& type, bool isParam = false, int paramIdx = -1);

    // ─────────────────────────────────────────────
    // AST code generation
    // ─────────────────────────────────────────────
    void genProgram(const std::shared_ptr<ASTNode>& node);
    void genStatement(const std::shared_ptr<ASTNode>& node);
    void genBlock(const std::shared_ptr<ASTNode>& node);
    void genVarDecl(const std::shared_ptr<ASTNode>& node);
    void genFuncDecl(const std::shared_ptr<ASTNode>& node);
    void genStructDecl(const std::shared_ptr<ASTNode>& node);
    void genReturn(const std::shared_ptr<ASTNode>& node);
    void genIf(const std::shared_ptr<ASTNode>& node);
    void genWhile(const std::shared_ptr<ASTNode>& node);
    void genFor(const std::shared_ptr<ASTNode>& node);
    void genLoop(const std::shared_ptr<ASTNode>& node);
    void genAssemblyBlock(const std::shared_ptr<ASTNode>& node);
    void genDirective(const std::shared_ptr<ASTNode>& node);
    void genSwitch(const std::shared_ptr<ASTNode>& node);

    // Expression codegen — result left in register 'dst'
    Reg genExpr(const std::shared_ptr<ASTNode>& node);
    Reg genBinaryExpr(const std::shared_ptr<ASTNode>& node);
    Reg genUnaryExpr(const std::shared_ptr<ASTNode>& node);
    Reg genCallExpr(const std::shared_ptr<ASTNode>& node);
    Reg genSyscallExpr(const std::shared_ptr<ASTNode>& node);
    Reg genCastExpr(const std::shared_ptr<ASTNode>& node);
    Reg genSizeofExpr(const std::shared_ptr<ASTNode>& node);
    Reg genAddressOfExpr(const std::shared_ptr<ASTNode>& node);
    Reg genDerefExpr(const std::shared_ptr<ASTNode>& node);
    Reg genAssignment(const std::shared_ptr<ASTNode>& node);
    Reg genCompoundAssignment(const std::shared_ptr<ASTNode>& node);
    Reg genMemberExpr(const std::shared_ptr<ASTNode>& node);
    Reg genIndexExpr(const std::shared_ptr<ASTNode>& node);
    Reg genMemoryExpr(const std::shared_ptr<ASTNode>& node);
    Reg genPortExpr(const std::shared_ptr<ASTNode>& node);
    Reg genRegisterExpr(const std::shared_ptr<ASTNode>& node);
    Reg genAsmInstruction(const AsmInstr& instr);

    // Label counter for unique labels
    int labelCounter_ = 0;
    std::string uniqueLabel(const std::string& prefix);

    // Break/continue label stack
    std::vector<std::string> breakLabels_;
    std::vector<std::string> continueLabels_;
};

} // namespace verslang
