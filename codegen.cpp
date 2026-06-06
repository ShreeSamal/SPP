#include "codegen.hpp"
#include <stdexcept>

static const char* INT_ARG_REGS[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

// ─── Output helpers ───────────────────────────────────────────────────────────
void CodeGen::emit(const std::string& line) { out << "    " << line << "\n"; }
void CodeGen::emitLabel(const std::string& l) { out << l << ":\n"; }

// ─── String interning — gives each unique string a .data label ───────────────
std::string CodeGen::internString(const std::string& content) {
    auto it = stringPool.find(content);
    if (it != stringPool.end()) return it->second;
    std::string label = "str" + std::to_string(stringCount++);
    stringPool[content] = label;

    // emit to data section — escape special chars for NASM
    dataSection << "    " << label << " db ";
    if (content.empty()) {
        dataSection << "0\n";
        return label;
    }
    // emit each character, handle newline/tab
    bool inString = false;
    for (unsigned char c : content) {
        if (c == '\n' || c == '\t' || c < 32 || c > 126) {
            if (inString) { dataSection << "\", "; inString = false; }
            dataSection << (int)c << ", ";
        } else {
            if (!inString) { dataSection << "\""; inString = true; }
            dataSection << c;
        }
    }
    if (inString) dataSection << "\", ";
    dataSection << "0\n";
    return label;
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

// ─── Collect function return types ────────────────────────────────────────────
void CodeGen::collectFnInfo(const Program& prog) {
    for (auto& fn : prog.fns) {
        std::string rt;
        switch (fn->returnType) {
            case TypeKind::FLOAT:  rt = "float";  break;
            case TypeKind::VOID:   rt = "void";   break;
            case TypeKind::STRING: rt = "string"; break;
            default:               rt = "int";    break;
        }
        fnReturnTypes[fn->name] = rt;
    }
}

// ─── Scan locals — pre-allocate all stack slots ────────────────────────────────
void CodeGen::scanLocals(const IRFunction& fn) {
    stackMap.clear();
    stackSize = 0;
    typeMap.clear();

    for (auto& ins : fn.instrs) {
        if (!ins.dst.empty()) allocSlot(ins.dst);

        // track string temps
        if (ins.op == IROp::ASSIGN_LIT && ins.src1 == "__str__")
            typeMap[ins.dst] = "string";

        if (!ins.src1.empty() &&
            ins.op != IROp::FUNC_BEGIN && ins.op != IROp::FUNC_END &&
            ins.op != IROp::GOTO       && ins.op != IROp::LABEL     &&
            ins.op != IROp::CALL) {
            bool isNum = !ins.src1.empty() &&
                         (isdigit((unsigned char)ins.src1[0]) || ins.src1[0] == '-');
            bool isStr = ins.src1 == "__str__";
            if (!isNum && !isStr) allocSlot(ins.src1);
        }
        if (!ins.src2.empty() && ins.op == IROp::BINARY) {
            bool isNum = isdigit((unsigned char)ins.src2[0]) || ins.src2[0] == '-';
            if (!isNum) allocSlot(ins.src2);
        }
    }
    if (stackSize % 16 != 0) stackSize += 8;
}

// ═════════════════════════════════════════════════════════════════════════════
// Entry point
// ═════════════════════════════════════════════════════════════════════════════
std::string CodeGen::generate(const IRProgram& ir, const Program& prog) {
    collectFnInfo(prog);

    // generate all functions first (string interning fills dataSection)
    std::ostringstream fnOut;
    for (auto& fn : ir.fns) {
        // swap out to fnOut temporarily
        std::ostringstream tmp;
        tmp << out.str();
        out.str(""); out.clear();
        genFunction(fn);
        tmp << out.str();
        out.str(""); out.clear();
        out << tmp.str();
    }

    // assemble final output: data section first, then text
    std::ostringstream final;
    final << "section .data\n";
    final << "    fmt_int   db \"%lld\", 10, 0\n";
    final << "    fmt_float db \"%f\",   10, 0\n";
    final << "    fmt_str   db \"%s\",   10, 0\n";
    final << dataSection.str();
    final << "\n";
    final << "section .text\n";
    final << "    global main\n";
    final << "    extern printf\n";
    final << "\n";
    final << out.str();
    return final.str();
}

// ═════════════════════════════════════════════════════════════════════════════
// Function
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genFunction(const IRFunction& fn) {
    pendingArgs.clear();
    scanLocals(fn);

    emitLabel(fn.name);
    emit("push rbp");
    emit("mov  rbp, rsp");
    emit("sub  rsp, " + std::to_string(stackSize));

    // spill parameters
    for (int i = 0; i < (int)fn.params.size() && i < 6; i++)
        emit("mov  " + slot(fn.params[i]) + ", " + INT_ARG_REGS[i]);

    for (auto& ins : fn.instrs)
        genInstr(ins);

    emit("xor  eax, eax");
    emit("leave");
    emit("ret");
    out << "\n";
}

// ═════════════════════════════════════════════════════════════════════════════
// Dispatch
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genInstr(const IRInstr& ins) {
    switch (ins.op) {
        case IROp::FUNC_BEGIN:
        case IROp::FUNC_END:
            break;

        case IROp::ASSIGN_LIT: {
            if (ins.src1 == "__str__") {
                // intern the string, store its address
                std::string lbl = internString(ins.op_str);
                emit("lea  rax, [rel " + lbl + "]");
                emit("mov  " + slot(ins.dst) + ", rax");
                typeMap[ins.dst] = "string";
            } else {
                bool isFloat = ins.src1.find('.') != std::string::npos;
                if (isFloat) {
                    union { double d; long long i; } u;
                    u.d = std::stod(ins.src1);
                    emit("mov  rax, " + std::to_string(u.i));
                    emit("mov  " + slot(ins.dst) + ", rax");
                } else {
                    emit("mov  " + slot(ins.dst) + ", " + ins.src1);
                }
            }
            break;
        }

        case IROp::ASSIGN_VAR: {
            // propagate string type
            if (typeMap.count(ins.src1))
                typeMap[ins.dst] = typeMap[ins.src1];
            emit("mov  rax, " + slot(ins.src1));
            emit("mov  " + slot(ins.dst) + ", rax");
            break;
        }

        case IROp::STORE: {
            if (typeMap.count(ins.src1))
                typeMap[ins.dst] = typeMap[ins.src1];
            emit("mov  rax, " + slot(ins.src1));
            emit("mov  " + slot(ins.dst) + ", rax");
            break;
        }

        case IROp::BINARY:  genBinary(ins);  break;
        case IROp::UNARY:   genUnary(ins);   break;
        case IROp::PARAM:   pendingArgs.push_back(ins.src1); break;
        case IROp::CALL:    genCall(ins);    break;
        case IROp::PRINT:   genPrint(ins);   break;
        case IROp::RETURN:  genReturn(ins);  break;
        case IROp::LABEL:   emitLabel(ins.src1); break;
        case IROp::GOTO:    emit("jmp  " + ins.src1); break;
        case IROp::IFFALSE:
            emit("cmp  " + slot(ins.src1) + ", 0");
            emit("je   " + ins.src2);
            break;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Binary
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genBinary(const IRInstr& ins) {
    const std::string& op = ins.op_str;

    if (op == "==" || op == "!=" || op == "<" || op == ">" ||
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
        emit("cqo");
        emit("idiv " + slot(ins.src2));
        emit("mov  " + slot(ins.dst) + ", rax");
    } else if (op == "%") {
        emit("mov  rax, " + slot(ins.src1));
        emit("cqo");
        emit("idiv " + slot(ins.src2));
        emit("mov  " + slot(ins.dst) + ", rdx");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Unary
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
// Call
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genCall(const IRInstr& ins) {
    int nArgs = std::stoi(ins.src2);
    for (int i = 0; i < nArgs && i < 6; i++)
        emit("mov  " + std::string(INT_ARG_REGS[i]) + ", " + slot(pendingArgs[i]));
    pendingArgs.clear();
    emit("call " + ins.src1);
    if (!ins.dst.empty()) {
        auto it = fnReturnTypes.find(ins.src1);
        if (it != fnReturnTypes.end() && it->second == "string") {
            typeMap[ins.dst] = "string";
        }
        emit("mov  " + slot(ins.dst) + ", rax");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Print — chooses format string based on type
// ═════════════════════════════════════════════════════════════════════════════
void CodeGen::genPrint(const IRInstr& ins) {
    bool isStr = typeMap.count(ins.src1) && typeMap[ins.src1] == "string";
    emit("mov  rsi, " + slot(ins.src1));
    if (isStr)
        emit("lea  rdi, [rel fmt_str]");
    else
        emit("lea  rdi, [rel fmt_int]");
    emit("xor  eax, eax");
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