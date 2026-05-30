#include "irgen.hpp"

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════
std::string IRGen::newTemp()  { return "t" + std::to_string(tempCount++);  }
std::string IRGen::newLabel() { return "L" + std::to_string(labelCount++); }

void IRGen::emit(IRInstr instr) {
    currentFn->instrs.push_back(std::move(instr));
}

// ═════════════════════════════════════════════════════════════════════════════
// Entry point
// ═════════════════════════════════════════════════════════════════════════════
IRProgram IRGen::generate(const Program& prog) {
    IRProgram ir;
    for (auto& fn : prog.fns) {
        ir.fns.push_back(IRFunction{});
        currentFn = &ir.fns.back();
        currentFn->name = fn->name;
        for (auto& p : fn->params)
            currentFn->params.push_back(p.name);
        currentFn = &ir.fns.back();
        genFunction(*fn);
    }
    return ir;
}

// ═════════════════════════════════════════════════════════════════════════════
// Function
// ═════════════════════════════════════════════════════════════════════════════
void IRGen::genFunction(const FnDecl& fn) {
    emit({IROp::FUNC_BEGIN, "", fn.name, "", ""});
    genBlock(*fn.body);
    emit({IROp::FUNC_END,   "", fn.name, "", ""});
}

// ═════════════════════════════════════════════════════════════════════════════
// Block
// ═════════════════════════════════════════════════════════════════════════════
void IRGen::genBlock(const Block& block) {
    for (auto& s : block.stmts)
        genStmt(*s);
}

// ═════════════════════════════════════════════════════════════════════════════
// Statements
// ═════════════════════════════════════════════════════════════════════════════
void IRGen::genStmt(const Stmt& s) {
    switch (s.kind) {

        // let x: T = expr  →  t0 = <expr>;  x = t0
        case StmtKind::LET: {
            std::string src = genExpr(*s.expr);
            emit({IROp::STORE, s.name, src, "", ""});
            break;
        }

        // x = expr  →  t0 = <expr>;  x = t0
        case StmtKind::ASSIGN: {
            std::string src = genExpr(*s.expr);
            emit({IROp::STORE, s.name, src, "", ""});
            break;
        }

        // if (cond) { then } else { else }
        //   t0      = <cond>
        //   iffalse t0 goto L_else
        //   <then>
        //   goto L_end
        // L_else:
        //   <else>
        // L_end:
        case StmtKind::IF: {
            std::string cond  = genExpr(*s.cond);
            std::string lElse = newLabel();
            std::string lEnd  = newLabel();

            emit({IROp::IFFALSE, "",     cond,  lElse, ""});
            genBlock(*s.thenBlock);
            emit({IROp::GOTO,    "",     lEnd,  "",    ""});
            emit({IROp::LABEL,   "",     lElse, "",    ""});
            if (s.elseBlock) genBlock(*s.elseBlock);
            emit({IROp::LABEL,   "",     lEnd,  "",    ""});
            break;
        }

        // while (cond) { body }
        // L_start:
        //   t0 = <cond>
        //   iffalse t0 goto L_end
        //   <body>
        //   goto L_start
        // L_end:
        case StmtKind::WHILE: {
            std::string lStart = newLabel();
            std::string lEnd   = newLabel();

            emit({IROp::LABEL,   "", lStart, "", ""});
            std::string cond = genExpr(*s.cond);
            emit({IROp::IFFALSE, "", cond,   lEnd, ""});
            genBlock(*s.thenBlock);
            emit({IROp::GOTO,    "", lStart, "", ""});
            emit({IROp::LABEL,   "", lEnd,   "", ""});
            break;
        }

        // return expr  →  t0 = <expr>;  return t0
        case StmtKind::RETURN: {
            std::string src = genExpr(*s.expr);
            emit({IROp::RETURN, "", src, "", ""});
            break;
        }

        // print(expr)  →  t0 = <expr>;  print t0
        case StmtKind::PRINT: {
            std::string src = genExpr(*s.expr);
            emit({IROp::PRINT, "", src, "", ""});
            break;
        }

        // bare expression statement (e.g. a void function call)
        case StmtKind::EXPR: {
            genExpr(*s.expr);
            break;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Expressions — returns the temp name holding the result
// ═════════════════════════════════════════════════════════════════════════════
std::string IRGen::genExpr(const Expr& e) {
    switch (e.kind) {

        // ── Literals ──────────────────────────────────────────────────────
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

        // ── Variable read ─────────────────────────────────────────────────
        case ExprKind::VAR: {
            std::string t = newTemp();
            emit({IROp::ASSIGN_VAR, t, e.name, "", ""});
            return t;
        }

        // ── Unary ─────────────────────────────────────────────────────────
        case ExprKind::UNARY: {
            std::string src = genExpr(*e.left);
            std::string t   = newTemp();
            emit({IROp::UNARY, t, src, "", e.op});
            return t;
        }

        // ── Binary ────────────────────────────────────────────────────────
        case ExprKind::BINARY: {
            std::string lhs = genExpr(*e.left);
            std::string rhs = genExpr(*e.right);
            std::string t   = newTemp();
            emit({IROp::BINARY, t, lhs, rhs, e.op});
            return t;
        }

        // ── Call ──────────────────────────────────────────────────────────
        // emit one PARAM per arg, then CALL
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
    return ""; // unreachable
}