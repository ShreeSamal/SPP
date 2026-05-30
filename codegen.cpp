#include "codegen.hpp"
#include <stdexcept>
#include <cassert>

// x86-64 System V ABI integer argument registers (first 6 args)
static const char* INT_ARG_REGS[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
// float argument registers
static const char* FLT_ARG_REGS[] = { "xmm0", "xmm1", "xmm2", "xmm3",
                                       "xmm4",  "xmm5", "xmm6", "xmm7" };

// ─── Output helpers ───────────────────────────────────────────────────────────
void CodeGen::emit(const std::string& line) {
    out << "    " << line << "\n";
}
void CodeGen::emitLabel(const std::string& label) {
    out << label << ":\n";
}

// ─── Stack slot management ────────────────────────────────────────────────────
int CodeGen::allocSlot(const std::string& name) {
    if (stackMap.count(name)) return stackMap[name];
    stackSize += 8;
    stackMap[name] = stackSize;
    return stackSize;
}

std::string CodeGen::slot(const std::string& name) {
    if (!stackMap.count(name))
        throw std::runtime_error("CodeGen: unknown slot '" + name + "'");
    return "qword [rbp - " + std::to_string(stackMap[name]) + "]";
}

// ─── Collect function return type info from Program ───────────────────────────
void CodeGen::collectFnInfo(const Program& prog) {
    for (auto& fn : prog.fns) {
        std::string rt;
        switch (fn->returnType) {
            case TypeKind::FLOAT: rt = "float"; break;
            case TypeKind::VOID:  rt = "void";  break;
            default:              rt = "int";   break;
        }
        fnReturnTypes[fn->name] = rt;
    }
}

// ─── Scan all locals/temps in a function to pre-allocate stack ────────────────
void CodeGen::scanLocals(const IRFunction& fn) {
    stackMap.clear();
    stackSize = 0;
    for (auto& ins : fn.instrs) {
        if (!ins.dst.empty())  allocSlot(ins.dst);
        if (!ins.src1.empty() && ins.src1[0] != 'L' &&
            ins.op != IROp::FUNC_BEGIN && ins.op != IROp::FUNC_END &&
            ins.op != IROp::GOTO && ins.op != IROp::LABEL &&
            ins.op != IROp::CALL) {
            // src1 might be a variable name (not a literal, not a label)
            // heuristic: if it's not a number and not empty, allocate
            bool isNum = !ins.src1.empty() &&
                         (isdigit(ins.src1[0]) || ins.src1[0] == '-');
            if (!isNum) allocSlot(ins.src1);
        }
        if (!ins.src2.empty() && ins.op == IROp::BINARY) {
            bool isNum = !ins.src2.empty() &&
                         (isdigit(ins.src2[0]) || ins.src2[0] == '-');
            if (!isNum) allocSlot(ins.src2);
        }
    }
    // align to 16 bytes
    if (stackSize % 16 != 0) stackSize += 8;
}

// ═════════════════════════════════════════════════════════════════════════════
// Entry point
// ═════════════════════════════════════════════════════════════════════════════
std::string CodeGen::generate(const IRProgram& ir, const Program& prog) {
    collectFnInfo(prog);

    // ── Data section ──────────────────────────────────────────────────────
    out << "section .data\n";
    out << "    fmt_int   db \"%lld\", 10, 0\n";
    out << "    fmt_float db \"%f\",   10, 0\n";
    out << "\n";

    // ── Text section ──────────────────────────────────────────────────────
    out << "section .text\n";
    out << "    global main\n";
    out << "    extern printf\n";
    out << "\n";

    for (auto& fn : ir.fns)
        genFunction(fn);

    return out.str();
}

// ═════════════════════════════════════════════════════════════════════════════
// Function prologue / epilogue
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genFunction(const IRFunction& fn) {
    pendingArgs.clear();
    scanLocals(fn);

    // label
    emitLabel(fn.name);

    // prologue
    emit("push rbp");
    emit("mov  rbp, rsp");
    emit("sub  rsp, " + std::to_string(stackSize));

    // spill parameters: rdi, rsi, rdx, rcx, r8, r9 → stack slots
    for (int i = 0; i < (int)fn.params.size() && i < 6; i++)
        emit("mov  " + slot(fn.params[i]) + ", " + INT_ARG_REGS[i]);

    // body
    for (auto& ins : fn.instrs)
        genInstr(ins);

    // implicit return 0 for void / main
    emit("xor  eax, eax");
    emit("leave");
    emit("ret");
    out << "\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// Dispatch one IR instruction
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genInstr(const IRInstr& ins) {
    switch (ins.op) {

        case IROp::FUNC_BEGIN:
        case IROp::FUNC_END:
            break; // handled by genFunction

        // dst = literal
        case IROp::ASSIGN_LIT: {
            // check if float literal
            bool isFloat = ins.src1.find('.') != std::string::npos;
            if (isFloat) {
                // store via xmm0
                emit("; float literal " + ins.src1);
                // use __float64__ constant trick via rax
                // encode with movq + xmm
                // simplest: use a .data constant — inline via hex
                // For simplicity: push as integer bits using double union
                union { double d; long long i; } u;
                u.d = std::stod(ins.src1);
                emit("mov  rax, " + std::to_string(u.i));
                emit("mov  " + slot(ins.dst) + ", rax");
            } else {
                emit("mov  " + slot(ins.dst) + ", " + ins.src1);
            }
            break;
        }

        // dst = src  (variable copy)
        case IROp::ASSIGN_VAR: {
            emit("mov  rax, " + slot(ins.src1));
            emit("mov  " + slot(ins.dst) + ", rax");
            break;
        }

        // named variable store: dst(name) = src
        case IROp::STORE: {
            emit("mov  rax, " + slot(ins.src1));
            emit("mov  " + slot(ins.dst) + ", rax");
            break;
        }

        case IROp::BINARY:  genBinary(ins);  break;
        case IROp::UNARY:   genUnary(ins);   break;

        // stage arg for upcoming call
        case IROp::PARAM:
            pendingArgs.push_back(ins.src1);
            break;

        case IROp::CALL:    genCall(ins);    break;
        case IROp::PRINT:   genPrint(ins);   break;
        case IROp::RETURN:  genReturn(ins);  break;

        case IROp::LABEL:
            emitLabel(ins.src1);
            break;

        case IROp::GOTO:
            emit("jmp  " + ins.src1);
            break;

        // iffalse src1 goto src2
        case IROp::IFFALSE:
            emit("cmp  " + slot(ins.src1) + ", 0");
            emit("je   " + ins.src2);
            break;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Binary operations
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genBinary(const IRInstr& ins) {
    const std::string& op  = ins.op_str;
    bool isFloat = false;

    // detect float: check if either slot was stored as float bits
    // simple heuristic: if op is +/-/*// and the IR came from float context
    // For now we check if the source came from a float literal by slot name
    // A robust impl would carry type info — for simplicity we use integer
    // ops for all binary, and float ops only for known float temps.
    // Since sema guarantees types match, we just need to know the type.
    // We use integer paths for int/bool, float for float.
    // We'll detect float by checking if the value in the slot looks like
    // a double bit pattern — instead let's just use integer arithmetic
    // for all non-float ops and use SSE for float ops.
    // For this compiler we track float vars by name prefix convention:
    // Not ideal — best fix is passing type map. We skip float binary for now
    // and handle it the same as int (works for comparisons, fails for arith).
    // TODO Phase 6: pass type annotation into codegen.
    (void)isFloat;

    // ── Comparison operators → result is 0 or 1 ──────────────────────────
    if (op == "==" || op == "!=" ||
        op == "<"  || op == ">"  ||
        op == "<=" || op == ">=") {
        emit("mov  rax, " + slot(ins.src1));
        emit("cmp  rax, " + slot(ins.src2));
        std::string setOp;
        if      (op == "==") setOp = "sete";
        else if (op == "!=") setOp = "setne";
        else if (op == "<")  setOp = "setl";
        else if (op == ">")  setOp = "setg";
        else if (op == "<=") setOp = "setle";
        else                 setOp = "setge";
        emit(setOp + " al");
        emit("movzx rax, al");
        emit("mov  " + slot(ins.dst) + ", rax");
        return;
    }

    // ── Logical && and || ─────────────────────────────────────────────────
    if (op == "&&") {
        emit("mov  rax, " + slot(ins.src1));
        emit("and  rax, " + slot(ins.src2));
        emit("mov  " + slot(ins.dst) + ", rax");
        return;
    }
    if (op == "||") {
        emit("mov  rax, " + slot(ins.src1));
        emit("or   rax, " + slot(ins.src2));
        emit("mov  " + slot(ins.dst) + ", rax");
        return;
    }

    // ── Arithmetic: +  -  *  /  % ────────────────────────────────────────
    if (op == "+") {
        emit("mov  rax, " + slot(ins.src1));
        emit("add  rax, " + slot(ins.src2));
        emit("mov  " + slot(ins.dst) + ", rax");
    } else if (op == "-") {
        emit("mov  rax, " + slot(ins.src1));
        emit("sub  rax, " + slot(ins.src2));
        emit("mov  " + slot(ins.dst) + ", rax");
    } else if (op == "*") {
        emit("mov  rax, " + slot(ins.src1));
        emit("imul rax, " + slot(ins.src2));
        emit("mov  " + slot(ins.dst) + ", rax");
    } else if (op == "/") {
        emit("mov  rax, " + slot(ins.src1));
        emit("cqo");                          // sign-extend rax into rdx:rax
        emit("idiv " + slot(ins.src2));       // rax = quotient
        emit("mov  " + slot(ins.dst) + ", rax");
    } else if (op == "%") {
        emit("mov  rax, " + slot(ins.src1));
        emit("cqo");
        emit("idiv " + slot(ins.src2));       // rdx = remainder
        emit("mov  " + slot(ins.dst) + ", rdx");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Unary operations
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genUnary(const IRInstr& ins) {
    if (ins.op_str == "-") {
        emit("mov  rax, " + slot(ins.src1));
        emit("neg  rax");
        emit("mov  " + slot(ins.dst) + ", rax");
    } else if (ins.op_str == "!") {
        emit("cmp  " + slot(ins.src1) + ", 0");
        emit("sete al");
        emit("movzx rax, al");
        emit("mov  " + slot(ins.dst) + ", rax");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Function call  — uses System V ABI
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genCall(const IRInstr& ins) {
    int nArgs = std::stoi(ins.src2);

    // load args into registers (up to 6)
    for (int i = 0; i < nArgs && i < 6; i++) {
        emit("mov  " + std::string(INT_ARG_REGS[i]) +
             ", " + slot(pendingArgs[i]));
    }
    pendingArgs.clear();

    // align stack to 16 bytes before call (ABI requirement)
    emit("call " + ins.src1);

    // store return value
    if (!ins.dst.empty()) {
        // check if function returns float
        auto it = fnReturnTypes.find(ins.src1);
        if (it != fnReturnTypes.end() && it->second == "float") {
            // xmm0 holds float return — store as bits via movq
            emit("movq rax, xmm0");
            emit("mov  " + slot(ins.dst) + ", rax");
        } else {
            emit("mov  " + slot(ins.dst) + ", rax");
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Print — calls printf with appropriate format string
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genPrint(const IRInstr& ins) {
    // We don't have type info here — we'd need to pass it through IR.
    // Default: treat as integer (int / bool).
    // For float: caller should have stored bits; we load into xmm0.
    // Use fmt_int for now; float printing needs type annotation (Phase 6 TODO).
    emit("mov  rsi, " + slot(ins.src1));
    emit("lea  rdi, [rel fmt_int]");
    emit("xor  eax, eax");    // al = 0: no xmm args to printf
    emit("call printf");
}

// ═════════════════════════════════════════════════════════════════════════════
// Return
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genReturn(const IRInstr& ins) {
    emit("mov  rax, " + slot(ins.src1));
    emit("leave");
    emit("ret");
}