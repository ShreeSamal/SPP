#pragma once
#include "ir.hpp"
#include "ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

class CodeGen {
public:
    // generate full .asm file content from IR + original program (for type info)
    std::string generate(const IRProgram& ir, const Program& prog);

private:
    // ── Output stream ─────────────────────────────────────────────────────
    std::ostringstream out;

    // ── Per-function state ────────────────────────────────────────────────
    std::unordered_map<std::string, int> stackMap;  // name → rbp offset (negative)
    int                                  stackSize;  // total bytes allocated
    std::vector<std::string>             pendingArgs; // staged params before call

    // type info per function (for float detection)
    // maps varname/temp → TypeKind — populated during function scan
    std::unordered_map<std::string, std::string> fnReturnTypes; // fnName → "int"|"float"|"bool"|"void"

    // ── Helpers ───────────────────────────────────────────────────────────
    void        collectFnInfo(const Program& prog);
    int         allocSlot(const std::string& name);   // get or create stack slot
    std::string slot(const std::string& name);         // "[rbp - N]"
    void        emit(const std::string& line);
    void        emitLabel(const std::string& label);

    // ── Per-function codegen ──────────────────────────────────────────────
    void genFunction(const IRFunction& fn);
    void scanLocals(const IRFunction& fn);             // pass 1: allocate all slots
    void genInstr(const IRInstr& ins);

    // ── Instruction helpers ───────────────────────────────────────────────
    void genBinary(const IRInstr& ins);
    void genUnary(const IRInstr& ins);
    void genCall(const IRInstr& ins);
    void genPrint(const IRInstr& ins);
    void genReturn(const IRInstr& ins);
};