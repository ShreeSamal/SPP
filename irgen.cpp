#include "irgen.hpp"

std::string IRGen::newTemp()  { return "t" + std::to_string(tempCount++); }
std::string IRGen::newLabel() { return "L" + std::to_string(labelCount++); }

void IRGen::emit(IRInstr instr) {
    currentFn->instrs.push_back(std::move(instr));
}

IRProgram IRGen::generate(const Program& prog) {
    IRProgram ir;
    for (auto& fn : prog.fns) {
        ir.fns.push_back(IRFunction{});
        currentFn = &ir.fns.back();
        currentFn->name = fn->name;
        for (auto& p : fn->params)
            currentFn->params.push_back(p.name);
        genFunction(*fn);
    }
    return ir;
}

void IRGen::genFunction(const FnDecl& fn) {
    emit({IROp::FUNC_BEGIN, "", fn.name, "", ""});
    genBlock(*fn.body);
    emit({IROp::FUNC_END,   "", fn.name, "", ""});
}

void IRGen::genBlock(const Block& block) {
    for (auto& s : block.stmts) genStmt(*s);
}

void IRGen::genStmt(const Stmt& s) {
    switch (s.kind) {
        case StmtKind::LET:
        case StmtKind::ASSIGN: {
            std::string src = genExpr(*s.expr);
            emit({IROp::STORE, s.name, src, "", ""});
            break;
        }
        case StmtKind::IF: {
            std::string cond  = genExpr(*s.cond);
            std::string lElse = newLabel();
            std::string lEnd  = newLabel();
            emit({IROp::IFFALSE, "", cond,  lElse, ""});
            genBlock(*s.thenBlock);
            emit({IROp::GOTO,    "", lEnd,  "",    ""});
            emit({IROp::LABEL,   "", lElse, "",    ""});
            if (s.elseBlock) genBlock(*s.elseBlock);
            emit({IROp::LABEL,   "", lEnd,  "",    ""});
            break;
        }
        case StmtKind::WHILE: {
            std::string lStart = newLabel();
            std::string lEnd   = newLabel();
            emit({IROp::LABEL,   "", lStart, "", ""});
            std::string cond = genExpr(*s.cond);
            emit({IROp::IFFALSE, "", cond, lEnd, ""});
            genBlock(*s.thenBlock);
            emit({IROp::GOTO,    "", lStart, "", ""});
            emit({IROp::LABEL,   "", lEnd,   "", ""});
            break;
        }
        case StmtKind::RETURN: {
            std::string src = genExpr(*s.expr);
            emit({IROp::RETURN, "", src, "", ""});
            break;
        }
        case StmtKind::PRINT: {
            std::string src = genExpr(*s.expr);
            emit({IROp::PRINT, "", src, "", ""});
            break;
        }
        case StmtKind::EXPR:
            genExpr(*s.expr);
            break;
    }
}

std::string IRGen::genExpr(const Expr& e) {
    switch (e.kind) {
        case ExprKind::LITERAL_INT: {
            std::string t = newTemp();
            emit({IROp::ASSIGN_LIT, t, std::to_string(e.ival), "", ""});
            return t;
        }
        case ExprKind::LITERAL_FLOAT: {
            std::string t = newTemp();
            emit({IROp::ASSIGN_LIT, t, std::to_string(e.fval), "", ""});
            return t;
        }
        case ExprKind::LITERAL_BOOL: {
            std::string t = newTemp();
            emit({IROp::ASSIGN_LIT, t, e.bval ? "1" : "0", "", ""});
            return t;
        }
        case ExprKind::LITERAL_STRING: {
            std::string t = newTemp();
            // op_str carries the actual string content
            emit({IROp::ASSIGN_LIT, t, "__str__", "", e.sval});
            return t;
        }
        case ExprKind::VAR: {
            std::string t = newTemp();
            emit({IROp::ASSIGN_VAR, t, e.name, "", ""});
            return t;
        }
        case ExprKind::UNARY: {
            std::string src = genExpr(*e.left);
            std::string t   = newTemp();
            emit({IROp::UNARY, t, src, "", e.op});
            return t;
        }
        case ExprKind::BINARY: {
            std::string lhs = genExpr(*e.left);
            std::string rhs = genExpr(*e.right);
            std::string t   = newTemp();
            emit({IROp::BINARY, t, lhs, rhs, e.op});
            return t;
        }
        case ExprKind::CALL: {
            for (auto& arg : e.args) {
                std::string a = genExpr(*arg);
                emit({IROp::PARAM, "", a, "", ""});
            }
            std::string t = newTemp();
            emit({IROp::CALL, t, e.name, std::to_string(e.args.size()), ""});
            return t;
        }
    }
    return "";
}