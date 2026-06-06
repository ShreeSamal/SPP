#include "sema.hpp"
#include <stdexcept>

// ─── Error ────────────────────────────────────────────────────────────────────
void SemanticAnalyzer::error(int line, const std::string& msg) {
    throw std::runtime_error("Semantic error at line " +
                             std::to_string(line) + ": " + msg);
}

// ═════════════════════════════════════════════════════════════════════════════
// Scope helpers
// ═════════════════════════════════════════════════════════════════════════════
void SemanticAnalyzer::pushScope() { scopes.push_back({}); }
void SemanticAnalyzer::popScope()  { scopes.pop_back();    }

bool SemanticAnalyzer::declaredInCurrentScope(const std::string& name) const {
    return !scopes.empty() && scopes.back().count(name);
}

void SemanticAnalyzer::declare(const std::string& name, TypeKind type, int line) {
    if (declaredInCurrentScope(name))
        error(line, "'" + name + "' is already declared in this scope");
    scopes.back()[name] = type;
}

TypeKind SemanticAnalyzer::lookup(const std::string& name, int line) const {
    for (int i = (int)scopes.size() - 1; i >= 0; i--) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) return it->second;
    }
    error(line, "undeclared variable '" + name + "'");
}

// ═════════════════════════════════════════════════════════════════════════════
// Pass 1 — collect all function signatures
// ═════════════════════════════════════════════════════════════════════════════
void SemanticAnalyzer::collectFunctions(const Program& prog) {
    for (auto& fn : prog.fns) {
        if (fnTable.count(fn->name))
            error(fn->line, "function '" + fn->name + "' already defined");
        FnSig sig;
        sig.returnType = fn->returnType;
        sig.line       = fn->line;
        for (auto& p : fn->params) {
            sig.paramTypes.push_back(p.type);
            sig.paramNames.push_back(p.name);
        }
        fnTable[fn->name] = std::move(sig);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Entry point
// ═════════════════════════════════════════════════════════════════════════════
void SemanticAnalyzer::analyze(const Program& prog) {
    collectFunctions(prog);
    for (auto& fn : prog.fns)
        checkFunction(*fn);
}

// ═════════════════════════════════════════════════════════════════════════════
// Pass 2 — check one function
// ═════════════════════════════════════════════════════════════════════════════
void SemanticAnalyzer::checkFunction(const FnDecl& fn) {
    currentReturnType = fn.returnType;
    pushScope();
    for (auto& p : fn.params)
        declare(p.name, p.type, fn.line);
    checkBlock(*fn.body);
    popScope();
}

// ═════════════════════════════════════════════════════════════════════════════
// Block
// ═════════════════════════════════════════════════════════════════════════════
void SemanticAnalyzer::checkBlock(const Block& block) {
    pushScope();
    for (auto& s : block.stmts)
        checkStmt(*s);
    popScope();
}

// ═════════════════════════════════════════════════════════════════════════════
// Statements
// ═════════════════════════════════════════════════════════════════════════════
void SemanticAnalyzer::checkStmt(const Stmt& s) {
    switch (s.kind) {

        case StmtKind::LET: {
            TypeKind initType = checkExpr(*s.expr);
            if (initType != s.varType)
                error(s.line, "cannot assign " +
                      std::string(typeKindName(initType)) + " to '" +
                      s.name + "' of type " +
                      std::string(typeKindName(s.varType)));
            declare(s.name, s.varType, s.line);
            break;
        }

        case StmtKind::ASSIGN: {
            TypeKind varType  = lookup(s.name, s.line);
            TypeKind exprType = checkExpr(*s.expr);
            if (varType != exprType)
                error(s.line, "cannot assign " +
                      std::string(typeKindName(exprType)) + " to '" +
                      s.name + "' of type " +
                      std::string(typeKindName(varType)));
            break;
        }

        case StmtKind::IF: {
            TypeKind condType = checkExpr(*s.cond);
            if (condType != TypeKind::BOOL)
                error(s.line, "if condition must be bool, got " +
                      std::string(typeKindName(condType)));
            checkBlock(*s.thenBlock);
            if (s.elseBlock) checkBlock(*s.elseBlock);
            break;
        }

        case StmtKind::WHILE: {
            TypeKind condType = checkExpr(*s.cond);
            if (condType != TypeKind::BOOL)
                error(s.line, "while condition must be bool, got " +
                      std::string(typeKindName(condType)));
            checkBlock(*s.thenBlock);
            break;
        }

        case StmtKind::RETURN: {
            TypeKind retType = checkExpr(*s.expr);
            if (retType != currentReturnType)
                error(s.line, "return type mismatch: expected " +
                      std::string(typeKindName(currentReturnType)) +
                      ", got " + std::string(typeKindName(retType)));
            break;
        }

        case StmtKind::PRINT: {
            TypeKind t = checkExpr(*s.expr);
            if (t == TypeKind::VOID)
                error(s.line, "cannot print a void expression");
            break;
        }

        case StmtKind::EXPR: {
            checkExpr(*s.expr);
            break;
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Expressions
// ═════════════════════════════════════════════════════════════════════════════
TypeKind SemanticAnalyzer::checkExpr(const Expr& e) {
    switch (e.kind) {
        case ExprKind::LITERAL_INT:    return TypeKind::INT;
        case ExprKind::LITERAL_FLOAT:  return TypeKind::FLOAT;
        case ExprKind::LITERAL_BOOL:   return TypeKind::BOOL;
        case ExprKind::LITERAL_STRING: return TypeKind::STRING;

        case ExprKind::VAR:
            return lookup(e.name, e.line);

        case ExprKind::CALL: {
            auto it = fnTable.find(e.name);
            if (it == fnTable.end())
                error(e.line, "call to undeclared function '" + e.name + "'");
            const FnSig& sig = it->second;
            if (e.args.size() != sig.paramTypes.size())
                error(e.line, "function '" + e.name + "' expects " +
                      std::to_string(sig.paramTypes.size()) +
                      " argument(s), got " + std::to_string(e.args.size()));
            for (size_t i = 0; i < e.args.size(); i++) {
                TypeKind argType = checkExpr(*e.args[i]);
                if (argType != sig.paramTypes[i])
                    error(e.line, "argument " + std::to_string(i + 1) +
                          " of '" + e.name + "' expects " +
                          std::string(typeKindName(sig.paramTypes[i])) +
                          ", got " + std::string(typeKindName(argType)));
            }
            return sig.returnType;
        }

        case ExprKind::UNARY: {
            TypeKind t = checkExpr(*e.left);
            if (e.op == "-") {
                if (t != TypeKind::INT && t != TypeKind::FLOAT)
                    error(e.line, "unary '-' requires int or float, got " +
                          std::string(typeKindName(t)));
                return t;
            }
            if (e.op == "!") {
                if (t != TypeKind::BOOL)
                    error(e.line, "unary '!' requires bool, got " +
                          std::string(typeKindName(t)));
                return TypeKind::BOOL;
            }
            error(e.line, "unknown unary operator '" + e.op + "'");
        }

        case ExprKind::BINARY: {
            TypeKind lhs = checkExpr(*e.left);
            TypeKind rhs = checkExpr(*e.right);

            if (e.op == "&&" || e.op == "||") {
                if (lhs != TypeKind::BOOL || rhs != TypeKind::BOOL)
                    error(e.line, "operator '" + e.op + "' requires bool operands");
                return TypeKind::BOOL;
            }
            if (e.op == "==" || e.op == "!=") {
                if (lhs != rhs)
                    error(e.line, "operator '" + e.op +
                          "' requires matching types, got " +
                          std::string(typeKindName(lhs)) + " and " +
                          std::string(typeKindName(rhs)));
                return TypeKind::BOOL;
            }
            if (e.op == "<" || e.op == ">" || e.op == "<=" || e.op == ">=") {
                if ((lhs != TypeKind::INT && lhs != TypeKind::FLOAT) || lhs != rhs)
                    error(e.line, "operator '" + e.op +
                          "' requires matching numeric operands, got " +
                          std::string(typeKindName(lhs)) + " and " +
                          std::string(typeKindName(rhs)));
                return TypeKind::BOOL;
            }
            if (e.op == "+" || e.op == "-" || e.op == "*" ||
                e.op == "/" || e.op == "%") {
                if (lhs != rhs || (lhs != TypeKind::INT && lhs != TypeKind::FLOAT))
                    error(e.line, "operator '" + e.op +
                          "' requires matching numeric operands, got " +
                          std::string(typeKindName(lhs)) + " and " +
                          std::string(typeKindName(rhs)));
                if (e.op == "%" && lhs != TypeKind::INT)
                    error(e.line, "operator '%' requires int operands");
                return lhs;
            }
            error(e.line, "unknown binary operator '" + e.op + "'");
        }
    }
    error(0, "unreachable");
}