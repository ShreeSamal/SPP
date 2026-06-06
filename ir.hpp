#pragma once
#include <string>
#include <vector>
#include <iostream>

// ─── IR Opcodes ───────────────────────────────────────────────────────────────
enum class IROp {
    ASSIGN_LIT,   // dst = literal
    ASSIGN_VAR,   // dst = src
    BINARY,       // dst = src1  op  src2
    UNARY,        // dst = op src1
    PARAM,        // param src1           (push arg before CALL)
    CALL,         // dst = call src1 (src2 args)
    STORE,        // dst(named var) = src1
    PRINT,        // print src1
    RETURN,       // return src1
    LABEL,        // src1:
    IFFALSE,      // iffalse src1 goto src2
    GOTO,         // goto src1
    FUNC_BEGIN,   // marker: start of function src1
    FUNC_END,     // marker: end of function src1
};

// ─── One IR instruction ───────────────────────────────────────────────────────
struct IRInstr {
    IROp        op;
    std::string dst;    // result temp or named var
    std::string src1;   // first operand / fn name / label / literal value
    std::string src2;   // second operand / arg count / jump target
    std::string op_str; // operator string for BINARY and UNARY
};

// ─── One function's IR ────────────────────────────────────────────────────────
struct IRFunction {
    std::vector<std::string> params;   // parameter names in ABI order
    std::string          name;
    std::vector<IRInstr> instrs;
};

// ─── Whole program IR ─────────────────────────────────────────────────────────
struct IRProgram {
    std::vector<IRFunction> fns;
};

// ─── Pretty printer ───────────────────────────────────────────────────────────
static inline void printIR(const IRProgram& prog) {
    for (auto& fn : prog.fns) {
        std::cout << "=== fn " << fn.name << " ===\n";
        for (auto& i : fn.instrs) {
            switch (i.op) {
                case IROp::FUNC_BEGIN: std::cout << "  [begin]\n";                                              break;
                case IROp::FUNC_END:   std::cout << "  [end]\n\n";                                             break;
                case IROp::ASSIGN_LIT: std::cout << "  " << i.dst << " = " << i.src1 << "\n";                  break;
                case IROp::ASSIGN_VAR: std::cout << "  " << i.dst << " = " << i.src1 << "\n";                  break;
                case IROp::BINARY:     std::cout << "  " << i.dst << " = " << i.src1
                                                 << " " << i.op_str << " " << i.src2 << "\n";                  break;
                case IROp::UNARY:      std::cout << "  " << i.dst << " = " << i.op_str << i.src1 << "\n";      break;
                case IROp::STORE:      std::cout << "  " << i.dst << " = " << i.src1 << "\n";                  break;
                case IROp::PARAM:      std::cout << "  param " << i.src1 << "\n";                              break;
                case IROp::CALL:
                    if (!i.dst.empty())
                         std::cout << "  " << i.dst << " = call " << i.src1 << "(" << i.src2 << ")\n";
                    else std::cout << "  call " << i.src1 << "(" << i.src2 << ")\n";
                    break;
                case IROp::PRINT:      std::cout << "  print " << i.src1 << "\n";                              break;
                case IROp::RETURN:     std::cout << "  return " << i.src1 << "\n";                             break;
                case IROp::LABEL:      std::cout << i.src1 << ":\n";                                           break;
                case IROp::IFFALSE:    std::cout << "  iffalse " << i.src1 << " goto " << i.src2 << "\n";      break;
                case IROp::GOTO:       std::cout << "  goto " << i.src1 << "\n";                               break;
            }
        }
    }
}