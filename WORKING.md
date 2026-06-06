# How S++ Works — Compiler Internals

A complete guide to how the S++ compiler is built and how each phase works.
Written so that someone who has never built a compiler can follow along.

---

## Table of Contents

1. [Big Picture](#big-picture)
2. [Phase 1 — Lexer](#phase-1--lexer)
3. [Phase 2 — Parser](#phase-2--parser)
4. [Phase 3 — Semantic Analyzer](#phase-3--semantic-analyzer)
5. [Phase 4 — IR Generator](#phase-4--ir-generator)
6. [Phase 5 — Code Generator](#phase-5--code-generator)
7. [End-to-End Walkthrough](#end-to-end-walkthrough)
8. [File Structure Reference](#file-structure-reference)

---

## Big Picture

A compiler is a program that translates source code into machine code.
The S++ compiler does this in 5 sequential phases. Each phase has a
well-defined input and output.

```
┌──────────────────────────────────────────────────────────────────┐
│                        S++ COMPILER                              │
│                                                                  │
│  hello.spp                                                       │
│  (source text)                                                   │
│      │                                                           │
│      ▼                                                           │
│  ┌─────────┐                                                     │
│  │  LEXER  │  reads characters → produces tokens                │
│  └─────────┘                                                     │
│      │  [LET, IDENT("x"), COLON, TYPE_INT, EQ, INT_LIT(10), ...]│
│      ▼                                                           │
│  ┌──────────┐                                                    │
│  │  PARSER  │  reads tokens → produces AST                      │
│  └──────────┘                                                    │
│      │  [FnDecl → Block → LetStmt → Binary(+) → ...]            │
│      ▼                                                           │
│  ┌──────────────────┐                                            │
│  │ SEMANTIC ANALYZER│  checks types and scopes                  │
│  └──────────────────┘                                            │
│      │  [verified AST — all types correct]                       │
│      ▼                                                           │
│  ┌──────────────┐                                                │
│  │ IR GENERATOR │  flattens AST → three-address code            │
│  └──────────────┘                                                │
│      │  [t0=10, t1=x, t2=t0+t1, print t2, ...]                  │
│      ▼                                                           │
│  ┌──────────────┐                                                │
│  │ CODE GENERATOR│  translates IR → x86-64 assembly             │
│  └──────────────┘                                                │
│      │                                                           │
│      ▼                                                           │
│  hello.asm                                                       │
│  (assembly)                                                      │
└──────────────────────────────────────────────────────────────────┘
         │
         │  nasm -f elf64 hello.asm -o hello.o
         │  gcc hello.o -o hello -no-pie
         ▼
     hello  (native binary — runs on CPU directly)
```

Each phase is completely independent. The output of one phase is
the only input to the next. This makes the compiler easy to debug —
you can inspect the output of any single phase.

---

## Phase 1 — Lexer

**Files:** `lexer.hpp`, `lexer.cpp`
**Input:** Raw source code string
**Output:** `std::vector<Token>`

### What is a token?

A token is the smallest meaningful unit in the language. The lexer
groups individual characters into tokens the same way you would group
the letters of a sentence into words and punctuation marks.

```
Source text:   let x: int = 10 + 2;

Characters:    l e t   x :   i n t   =   1 0   +   2 ;
               │─────│ │ │   │─────│ │   │──│   │   │ │
Tokens:         LET   IDENT  TYPE_INT EQ  INT  PLUS INT SEMI
                      "x"             "=" "10"     "2"
```

### Token structure

Every token carries three pieces of information:

```cpp
struct Token {
    TokenType   type;    // what kind of token is this?
    std::string value;   // the raw text: "let", "42", "+", "myVar"
    int         line;    // which source line it came from (for errors)
};
```

### All token types

```
Keywords:    LET, FN, IF, ELSE, WHILE, RETURN, PRINT
Types:       TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_VOID, TYPE_STRING
Literals:    INT_LIT, FLOAT_LIT, BOOL_LIT, STRING_LIT
Identifier:  IDENT
Arithmetic:  PLUS(+)  MINUS(-)  STAR(*)  SLASH(/)  PERCENT(%)
Comparison:  EQ_EQ(==)  BANG_EQ(!=)  LT(<)  GT(>)  LT_EQ(<=)  GT_EQ(>=)
Logical:     AMP_AMP(&&)  PIPE_PIPE(||)  BANG(!)
Punctuation: EQ(=)  LPAREN(()  RPAREN())  LBRACE({)  RBRACE(})
             COLON(:)  SEMICOLON(;)  COMMA(,)  ARROW(->)
Special:     EOF_TOKEN
```

### How the lexer works — character by character

The lexer has a position pointer `pos` that walks through the source string.
At each step it looks at `src[pos]` and decides what to do:

```
void loop:
    skip whitespace and // comments
    if pos >= src.size() → emit EOF, stop
    if current char is letter or _  → readIdentifierOrKeyword()
    if current char is digit        → readNumber()
    if current char is '"'          → readString()
    else                            → readSymbol()
```

**Reading an identifier or keyword:**
```
Start at 'a', keep consuming while char is letter/digit/_
Result string: "add", "myVar", "x123"
Then check against keyword table:
  "let"    → LET
  "fn"     → FN
  "if"     → IF
  "int"    → TYPE_INT
  "true"   → BOOL_LIT
  "string" → TYPE_STRING
  anything else → IDENT
```

**Reading a number:**
```
Consume digits: "123"
If next char is '.' and char after that is digit:
    consume '.' and more digits: "3.14" → FLOAT_LIT
else:
    "123" → INT_LIT
```


**Reading a string literal:**
```
See '"' → consume it (opening quote)
Loop: consume characters until closing '"'
    if '\\' seen → read next char as escape:
        'n'  → newline character
        't'  → tab character
        '\\' → backslash
        '"'  → double quote
Consume closing '"'
Return STRING_LIT with the unescaped content
```
Error if end of file is reached before closing quote:
```
Source: let s: string = "unterminated;
Error:  Lexer error: Unterminated string at line 1
```

**Reading a symbol:**
The lexer consumes one character then peeks at the next to handle
two-character operators:

```
See '='  → peek next:
    next is '=' → consume both → EQ_EQ ("==")
    otherwise   → EQ ("=")

See '<'  → peek next:
    next is '=' → consume both → LT_EQ ("<=")
    otherwise   → LT ("<")

See '-'  → peek next:
    next is '>' → consume both → ARROW ("->")
    otherwise   → MINUS ("-")

See '&'  → peek next:
    next is '&' → consume both → AMP_AMP ("&&")

See '/'  → peek next:
    next is '/' → skip to end of line (comment)
    otherwise   → SLASH ("/")
```

**Skipping whitespace and comments:**
```cpp
while (pos < src.size()) {
    if current is space/tab/newline → advance()
    if current is '/' and next is '/' → advance until '\n'
    else break
}
```

### Full lexer output example

```
Source:
    fn add(a: int, b: int) -> int {
        return a + b;
    }

Tokens:
    [line 1] FN        'fn'
    [line 1] IDENT     'add'
    [line 1] LPAREN    '('
    [line 1] IDENT     'a'
    [line 1] COLON     ':'
    [line 1] TYPE_INT  'int'
    [line 1] COMMA     ','
    [line 1] IDENT     'b'
    [line 1] COLON     ':'
    [line 1] TYPE_INT  'int'
    [line 1] RPAREN    ')'
    [line 1] ARROW     '->'
    [line 1] TYPE_INT  'int'
    [line 1] LBRACE    '{'
    [line 2] RETURN    'return'
    [line 2] IDENT     'a'
    [line 2] PLUS      '+'
    [line 2] IDENT     'b'
    [line 2] SEMICOLON ';'
    [line 3] RBRACE    '}'
    [line 3] EOF       'EOF'
```

### Error handling

If the lexer encounters a character it doesn't recognize:
```
Source: let x@ = 5;
Error:  Lexer error: Unknown character '@' at line 1
```

---

## Phase 2 — Parser

**Files:** `parser.hpp`, `parser.cpp`, `ast.hpp`
**Input:** `std::vector<Token>`
**Output:** `Program` (the root AST node)

### What is an AST?

An AST (Abstract Syntax Tree) is a tree where:
- Each **node** represents a language construct
- **Children** represent sub-constructs
- The **root** is the entire program

The tree captures the nesting and relationships in the code that
the flat token list cannot express.

```
Source:   let x: int = a + b * 2;

Token list (flat):
    LET  IDENT("x")  COLON  TYPE_INT  EQ  IDENT("a")  PLUS
    IDENT("b")  STAR  INT_LIT(2)  SEMICOLON

AST (shows structure and precedence):
    LetStmt
    ├── name: "x"
    ├── type: int
    └── init:
        Binary(+)
        ├── Var("a")
        └── Binary(*)
            ├── Var("b")
            └── IntLit(2)
```

The tree immediately shows that `*` has higher precedence than `+`
(it is deeper in the tree). The flat token list cannot show this.

### AST node types

**Program** — the root, contains all function declarations:
```cpp
struct Program {
    std::vector<std::unique_ptr<FnDecl>> fns;
};
```

**FnDecl** — one function:
```cpp
struct FnDecl {
    std::string        name;        // "add"
    std::vector<Param> params;      // [{a, int}, {b, int}]
    TypeKind           returnType;  // INT
    BlockPtr           body;        // the { ... }
    int                line;
};
```

**Block** — a sequence of statements inside `{ }`:
```cpp
struct Block {
    std::vector<StmtPtr> stmts;
};
```

**Stmt** — one statement. All statement kinds share one struct,
with a `kind` field to distinguish them:
```cpp
struct Stmt {
    StmtKind kind;   // LET, ASSIGN, IF, WHILE, RETURN, PRINT, EXPR

    // LET and ASSIGN
    std::string name;
    TypeKind    varType;
    ExprPtr     expr;

    // IF and WHILE
    ExprPtr  cond;
    BlockPtr thenBlock;
    BlockPtr elseBlock;   // null if no else
};
```

**Expr** — one expression. All expression kinds share one struct:
```cpp
struct Expr {
    ExprKind kind;   // LITERAL_INT, LITERAL_FLOAT, LITERAL_BOOL, LITERAL_STRING,
                     // VAR, BINARY, UNARY, CALL

    long long   ival;   // for LITERAL_INT
    double      fval;   // for LITERAL_FLOAT
    bool        bval;   // for LITERAL_BOOL
    std::string sval;   // for LITERAL_STRING (the unescaped content)
    std::string name;   // for VAR and CALL
    std::string op;     // for BINARY and UNARY: "+", "-", "=="...
    ExprPtr     left;   // for BINARY (left side) and UNARY (operand)
    ExprPtr     right;  // for BINARY (right side)
    std::vector<ExprPtr> args;  // for CALL
};
```

### Recursive descent parsing

The parser uses **recursive descent** — one function per grammar rule.
Each function consumes tokens and returns the corresponding AST node.

The key navigation methods:

```cpp
Token& current()              // look at current token without consuming
Token  advance()              // consume and return current token
bool   check(TokenType t)     // is current token of type t?
bool   match(TokenType t)     // if check(t), advance and return true
Token  expect(TokenType t, msg) // advance if check(t), else throw error
```

### Grammar and how each rule is parsed

**Program** — zero or more function declarations until EOF:
```cpp
Program parse() {
    Program prog;
    while (!check(EOF_TOKEN))
        prog.fns.push_back(parseFnDecl());
    return prog;
}
```

**Function declaration:**
```
fn add(a: int, b: int) -> int { ... }
│  │   │──────────────│    │    │   │
│  │   params              │    └── body (block)
│  │                       return type
│  name
keyword
```
```cpp
FnDecl parseFnDecl() {
    expect(FN)
    name = expect(IDENT)
    expect(LPAREN)
    params = parseParams()
    expect(RPAREN)
    expect(ARROW)
    returnType = parseType()
    body = parseBlock()
}
```

**Block** — statements between `{` and `}`:
```cpp
Block parseBlock() {
    expect(LBRACE)
    while (!check(RBRACE))
        stmts.push_back(parseStmt())
    expect(RBRACE)
}
```

**Statement** — dispatch on the current token:
```cpp
Stmt parseStmt() {
    switch (current().type) {
        case LET:    return parseLetStmt();
        case IF:     return parseIfStmt();
        case WHILE:  return parseWhileStmt();
        case RETURN: return parseReturnStmt();
        case PRINT:  return parsePrintStmt();
        default:     return parseAssignOrExprStmt();
    }
}
```

**Let statement:**
```
let x: int = expr;
│   │  │      │
│   │  type   initializer expression
│   name
keyword
```
```cpp
Stmt parseLetStmt() {
    expect(LET)
    name = expect(IDENT)
    expect(COLON)
    type = parseType()
    expect(EQ)
    init = parseExpr()
    expect(SEMICOLON)
    return makeLetStmt(name, type, init)
}
```

**Assign or expression statement** — needs one token of lookahead:
```
If current is IDENT and NEXT is EQ (not EQ_EQ):
    x = expr;    → AssignStmt
Else:
    expr;        → ExprStmt (e.g. a function call used as statement)
```

**If statement:**
```
if (cond) { then } else if (cond) { ... } else { else }
│   │                   │                        │
│   expr                another if_stmt (recurse) block (optional)
keyword
```

The key insight: after `else`, the parser checks if the next token
is `if`. If so, it calls `parseIfStmt()` recursively and wraps
the result in a synthetic block. This gives unlimited `else if`
chaining with no special grammar rule needed:
```cpp
if (match(ELSE)) {
    if (check(IF)) {
        // else if → parse nested if, wrap in block
        auto nested = parseIfStmt();
        elseBlock = make_unique<Block>();
        elseBlock->stmts.push_back(move(nested));
    } else {
        elseBlock = parseBlock();  // plain else { }
    }
}
```

**While statement:**
```
while (cond) { body }
│      │       │
│      expr    block
keyword
```

### Expression parsing and operator precedence

This is the most important part of the parser. Operator precedence
is encoded by the **depth of function calls** — functions lower in
the call chain bind tighter.

```
parseExpr()          ← lowest precedence
  └── parseOr()              ||
        └── parseAnd()       &&
              └── parseEquality()     == !=
                    └── parseComparison()  < > <= >=
                          └── parseAddSub()    + -
                                └── parseMulDiv()  * / %
                                      └── parseUnary()  ! -
                                            └── parsePrimary()  ← highest
```

Each level follows the same pattern:

```cpp
ExprPtr parseAddSub() {
    ExprPtr left = parseMulDiv();      // parse higher-precedence first
    while (check(PLUS) || check(MINUS)) {
        op = advance().value;          // consume operator
        right = parseMulDiv();         // parse right side
        left = makeBinary(op, left, right);  // fold into tree
    }
    return left;
}
```

Because `parseMulDiv` is called before handling `+`/`-`, multiply
always binds tighter — it is deeper in the tree.

**Example: `2 + 3 * 4 - 1`**

```
Call stack:
parseAddSub()
  left = parseMulDiv()
    left = parseUnary() → IntLit(2)
    no * or / → return IntLit(2)
  see '+', advance
  right = parseMulDiv()
    left = parseUnary() → IntLit(3)
    see '*', advance
    right = parseUnary() → IntLit(4)
    return Binary(*, 3, 4)
  left = Binary(+, 2, Binary(*, 3, 4))
  see '-', advance
  right = parseMulDiv()
    return IntLit(1)
  left = Binary(-, Binary(+, 2, Binary(*, 3, 4)), 1)
  no more + or - → return

Result AST:
    Binary(-)
    ├── Binary(+)
    │   ├── IntLit(2)
    │   └── Binary(*)
    │       ├── IntLit(3)
    │       └── IntLit(4)
    └── IntLit(1)
```

**Primary expressions** — the leaves of the tree:
```cpp
ExprPtr parsePrimary() {
    if INT_LIT  → return makeIntLit(stoll(value))
    if FLOAT_LIT → return makeFloatLit(stod(value))
    if BOOL_LIT  → return makeBoolLit(value == "true")
    if STRING_LIT → return makeStringLit(value)  // sval = unescaped content
    if IDENT:
        name = advance()
        if next is '(':          // function call
            parse args until ')'
            return makeCall(name, args)
        else:                    // variable reference
            return makeVar(name)
    if '(':                      // grouped expression
        expr = parseExpr()
        expect(')')
        return expr
}
```

### Full AST output example

```
Source:
    fn factorial(n: int) -> int {
        let result: int = 1;
        while (n > 1) {
            result = result * n;
            n = n - 1;
        }
        return result;
    }

AST:
    FnDecl(factorial) -> int
      Params: n: int
      Body:
        LetStmt(result: int)
          IntLit(1)
        WhileStmt
          Cond:
            Binary(>)
              Var(n)
              IntLit(1)
          Body:
            AssignStmt(result)
              Binary(*)
                Var(result)
                Var(n)
            AssignStmt(n)
              Binary(-)
                Var(n)
                IntLit(1)
        ReturnStmt
          Var(result)
```

### Error handling

Parser errors include the line number and the problematic token:
```
Source:  let x int = 5;   // missing colon
Error:   Parse error at line 1 ('int'): expected ':'

Source:  fn add(a: int b: int)  // missing comma
Error:   Parse error at line 1 ('b'): expected ')'
```

---

## Phase 3 — Semantic Analyzer

**Files:** `sema.hpp`, `sema.cpp`
**Input:** `Program` (AST)
**Output:** throws on error, or passes silently

### What is semantic analysis?

The parser only checks **syntax** — whether the code is grammatically valid.
Semantic analysis checks **meaning** — whether the code makes logical sense.

```
fn main() -> void {
    let x: int = true;   // syntactically valid, semantically wrong
}
// Parser: OK (it's valid grammar)
// Semantic: ERROR — cannot assign bool to 'x' of type int
```

### Two-pass design

The analyzer runs two passes over the program:

**Pass 1 — Collect all function signatures**

Before checking any function body, every function's name, parameter
types, and return type are recorded in a `fnTable`. This is what
allows functions to call each other regardless of definition order:

```cpp
// pass 1 builds this table:
fnTable["add"]  = { returnType: INT,  params: [INT, INT] }
fnTable["main"] = { returnType: VOID, params: [] }

// now main() can call add() even if add() appears after main() in source
```

**Pass 2 — Check each function body**

Each function body is walked recursively. The semantic analyzer
carries two pieces of state:

1. `currentReturnType` — the declared return type of the function
   being checked (used to validate `return` statements)
2. `scopes` — a stack of variable maps (described below)

### The scope stack

Variables are tracked in a **stack of scopes**. Each `{` pushes a
new empty scope onto the stack. Each `}` pops it. When looking up
a variable, the stack is searched from top (innermost) to bottom
(outermost):

```
fn main() -> void {            // push scope [0]
    let x: int = 10;           // scope[0] = {x: int}
    if (x > 5) {               // push scope [1]
        let y: int = 20;       // scope[1] = {y: int}
        // lookup(x) → found in scope[0] ✓
        // lookup(y) → found in scope[1] ✓
    }                          // pop scope [1] — y is gone
    // lookup(y) here → NOT FOUND → error
}                              // pop scope [0]
```

Function parameters are declared into the function's outermost scope
before the body is checked:

```
fn add(a: int, b: int) -> int {
    // scope[0] starts as {a: int, b: int}
    // (parameters are pre-declared)
    return a + b;
}
```

### Type checking expressions

`checkExpr()` returns the `TypeKind` of the expression. This lets
types propagate upward from leaves to the root:

```
Expression:  a + b   (where a: int, b: int)

checkExpr(Binary(+))
  lhs = checkExpr(Var("a"))
      → lookup("a") in scopes → int
  rhs = checkExpr(Var("b"))
      → lookup("b") in scopes → int
  op is "+" — requires matching numeric types
  lhs (int) == rhs (int) ✓
  int is numeric ✓
  → returns int
```

**Type rules for each operator:**

```
Arithmetic (+, -, *, /):
  Both sides must be the same type (int or float)
  Result is the same type

Modulo (%):
  Both sides must be int
  Result is int

Comparison (<, >, <=, >=):
  Both sides must be the same numeric type
  Result is bool

Equality (==, !=):
  Both sides must be the same type (any type)
  Result is bool

Logical (&&, ||):
  Both sides must be bool
  Result is bool

Unary minus (-):
  Operand must be int or float
  Result is same type

Unary not (!):
  Operand must be bool
  Result is bool
```

### All checks performed

**1. Undeclared variable:**
```
fn main() -> void { print(z); }
Error: Semantic error at line 1: undeclared variable 'z'
```

**2. Duplicate declaration:**
```
fn main() -> void {
    let x: int = 1;
    let x: int = 2;
}
Error: Semantic error at line 3: 'x' already declared in this scope
```

**3. Undeclared function:**
```
fn main() -> void { let x: int = foo(1); }
Error: Semantic error at line 1: call to undeclared function 'foo'
```

**4. Wrong argument count:**
```
fn add(a: int, b: int) -> int { return a + b; }
fn main() -> void { let x: int = add(1, 2, 3); }
Error: Semantic error at line 2: function 'add' expects 2 argument(s), got 3
```

**5. Type mismatch in let:**
```
fn main() -> void { let x: int = 3.14; }
Error: Semantic error at line 1: cannot assign float to 'x' of type int
```

**6. Type mismatch in assignment:**
```
fn main() -> void { let x: int = 10; x = true; }
Error: Semantic error at line 1: cannot assign bool to 'x' of type int
```

**7. Return type mismatch:**
```
fn getNum() -> int { return true; }
Error: Semantic error at line 1: return type mismatch: expected int, got bool
```

**8. Non-bool condition:**
```
fn main() -> void { let x: int = 5; if (x) { print(x); } }
Error: Semantic error at line 1: if condition must be bool, got int
```

**9. Wrong argument type:**
```
fn add(a: int, b: int) -> int { return a + b; }
fn main() -> void { let x: int = add(1, true); }
Error: Semantic error at line 2: argument 2 of 'add' expects int, got bool
```

**10. Arithmetic on wrong type:**
```
fn main() -> void { let t: bool = true; let x: int = t + 1; }
Error: Semantic error at line 1: operator '+' requires matching numeric operands, got bool and int
```

**11. Duplicate function:**
```
fn foo() -> void { }
fn foo() -> void { }
Error: Semantic error at line 2: function 'foo' already defined
```

---

## Phase 4 — IR Generator

**Files:** `ir.hpp`, `irgen.hpp`, `irgen.cpp`
**Input:** `Program` (validated AST)
**Output:** `IRProgram` (list of `IRFunction`, each containing `IRInstr` list)

### What is IR?

IR (Intermediate Representation) is a simplified, linear form of
the program that sits between the AST and assembly. It has two goals:

1. **Flatten** the tree — remove nesting so every instruction does
   exactly one thing
2. **Linearize** control flow — turn if/while into labels and jumps

The IR used by S++ is called **three-address code** because each
instruction has at most three operands: one destination and two sources.

```
dst = src1  OP  src2
```

### IR instruction set

```
ASSIGN_LIT    t0 = 42              load a literal value into a temp
ASSIGN_VAR    t1 = x               copy a variable into a temp
BINARY        t2 = t0 + t1         binary operation on two temps
UNARY         t3 = -t0             unary operation on a temp
STORE         x = t2               write a temp back to a named variable
PARAM         param t0             stage an argument for the next call
CALL          t4 = call add(2)     call function, result in temp
PRINT         print t0             print a temp's value
RETURN        return t0            return a temp's value
LABEL         L0:                  jump target
IFFALSE       iffalse t0 goto L1   jump if temp is 0 (false)
GOTO          goto L0              unconditional jump
FUNC_BEGIN    [begin]              marker: start of function
FUNC_END      [end]                marker: end of function
```

### Temporaries

Every sub-expression result is stored in a fresh **temporary variable**
named `t0`, `t1`, `t2`, ... Temporaries are allocated in order and
never reused.

```
Source:  let x: int = (a + b) * factorial(n) - 1;

IR:
  t0 = a                 // load a
  t1 = b                 // load b
  t2 = t0 + t1           // a + b
  t3 = n                 // load n (arg to factorial)
  param t3               // stage arg
  t4 = call factorial(1) // call factorial
  t5 = t2 * t4           // (a+b) * factorial(n)
  t6 = 1                 // literal 1
  t7 = t5 - t6           // subtract 1
  x = t7                 // store to x
```

Each instruction is trivially simple. No nesting, no ambiguity.

### Generating IR from expressions

`genExpr(expr)` recursively generates IR for an expression and returns
the name of the temporary holding the result:

**Literal:**
```
genExpr(IntLit(42)):
  t = newTemp()         // allocate t0
  emit: t0 = 42
  return "t0"
```

**Variable:**
```
genExpr(Var("x")):
  t = newTemp()         // allocate t1
  emit: t1 = x
  return "t1"
```

**Binary expression:**
```
genExpr(Binary(+, Var(a), Var(b))):
  lhs = genExpr(Var(a))    → emit: t0 = a,  returns "t0"
  rhs = genExpr(Var(b))    → emit: t1 = b,  returns "t1"
  t = newTemp()             → t2
  emit: t2 = t0 + t1
  return "t2"
```

**Function call:**
```
genExpr(Call("add", [Var(x), IntLit(5)])):
  a0 = genExpr(Var(x))     → emit: t0 = x,  returns "t0"
  emit: param t0
  a1 = genExpr(IntLit(5))  → emit: t1 = 5,  returns "t1"
  emit: param t1
  t = newTemp()             → t2
  emit: t2 = call add(2)
  return "t2"
```


**String literal:**
```
genExpr(StringLit("hello")):
  t = newTemp()         // allocate t0
  // op_str carries the actual content
  emit: t0 = __str__   (op_str = "hello")
  return "t0"
```
The sentinel value `__str__` in `src1` signals to the code generator
that this is a string — not a numeric literal. The actual content
travels in the `op_str` field of `IRInstr`.

### Generating IR for control flow

Control flow is lowered to labels and jumps using a label counter:

**If / else:**
```
Source:
  if (x > 0) {
      print(1);
  } else {
      print(0);
  }

IR generation steps:
  1. generate condition expression
  2. allocate two labels: L_else, L_end
  3. emit: iffalse <cond> goto L_else
  4. generate then-block
  5. emit: goto L_end
  6. emit: L_else:
  7. generate else-block (if present)
  8. emit: L_end:

Result IR:
  t0 = x
  t1 = 0
  t2 = t0 > t1
  iffalse t2 goto L0     ← if false, jump to else
  t3 = 1
  print t3               ← then-block
  goto L1                ← skip else
L0:
  t4 = 0
  print t4               ← else-block
L1:
```

**While loop:**
```
Source:
  while (i <= 5) {
      print(i);
      i = i + 1;
  }

IR generation steps:
  1. allocate two labels: L_start, L_end
  2. emit: L_start:
  3. generate condition expression
  4. emit: iffalse <cond> goto L_end
  5. generate body
  6. emit: goto L_start
  7. emit: L_end:

Result IR:
L0:
  t0 = i
  t1 = 5
  t2 = t0 <= t1
  iffalse t2 goto L1     ← exit when condition is false
  t3 = i
  print t3
  t4 = i
  t5 = 1
  t6 = t4 + t5
  i = t6
  goto L0                ← back to top
L1:
```

### Full IR output example

```
Source:
  fn add(a: int, b: int) -> int {
      return a + b;
  }

  fn main() -> void {
      let x: int = add(3, 4);
      print(x);
  }

IR:
  === fn add ===
    [begin]
    t0 = a
    t1 = b
    t2 = t0 + t1
    return t2
    [end]

  === fn main ===
    [begin]
    t3 = 3
    param t3
    t4 = 4
    param t4
    t5 = call add(2)
    x = t5
    t6 = x
    print t6
    [end]
```

---

## Phase 5 — Code Generator

**Files:** `codegen.hpp`, `codegen.cpp`
**Input:** `IRProgram` + `Program` (for type info)
**Output:** NASM assembly string written to `.asm` file

### What is x86-64 assembly?

Assembly is the lowest-level human-readable programming language.
Each assembly instruction maps directly to one CPU operation.

```nasm
mov rax, 42       ; put the number 42 into register rax
add rax, rbx      ; add rbx to rax, store result in rax
cmp rax, 0        ; compare rax with 0 (sets flags)
je  some_label    ; jump to some_label if last comparison was equal
call printf       ; call the printf function
```

### Registers

The x86-64 CPU has 16 general-purpose 64-bit registers.
S++ uses these:

```
rax   — primary scratch register, return values
rbx   — secondary scratch
rcx   — scratch
rdx   — scratch, also rdx:rax for division
rbp   — base pointer (bottom of current stack frame)
rsp   — stack pointer (top of call stack)
rdi   — 1st function argument
rsi   — 2nd function argument
rdx   — 3rd function argument
rcx   — 4th function argument
r8    — 5th function argument
r9    — 6th function argument
```

### The stack frame

Every function gets its own **stack frame** — a region of the call
stack used to store all local variables and temporaries.

```
Memory layout when inside a function:

High address
┌──────────────────────────────┐
│  ...caller's frame...        │
├──────────────────────────────┤  ← rbp points here
│  saved rbp (8 bytes)         │  [rbp + 0]  (previous rbp)
├──────────────────────────────┤
│  variable 'a'   (8 bytes)    │  [rbp - 8]
├──────────────────────────────┤
│  variable 'b'   (8 bytes)    │  [rbp - 16]
├──────────────────────────────┤
│  temp 't0'      (8 bytes)    │  [rbp - 24]
├──────────────────────────────┤
│  temp 't1'      (8 bytes)    │  [rbp - 32]
├──────────────────────────────┤
│  ...more slots...            │
└──────────────────────────────┘  ← rsp points here
Low address
```

Every variable and temporary gets its own 8-byte slot. The codegen
builds a `stackMap` — a dictionary from name to byte offset:

```
stackMap["a"]  = 8   → slot is [rbp - 8]
stackMap["b"]  = 16  → slot is [rbp - 16]
stackMap["t0"] = 24  → slot is [rbp - 24]
```

### Function prologue and epilogue

Every function starts with a **prologue** and ends with an **epilogue**:

```nasm
add:
    ; ── PROLOGUE ──────────────────────────────
    push rbp          ; save caller's frame pointer on stack
    mov  rbp, rsp     ; set our frame pointer to current stack top
    sub  rsp, 48      ; grow stack down to make room for all locals

    ; spill incoming parameters from registers to stack
    mov  qword [rbp - 8],  rdi   ; parameter 'a' arrived in rdi
    mov  qword [rbp - 16], rsi   ; parameter 'b' arrived in rsi

    ; ── BODY ──────────────────────────────────
    ; ... generated instructions ...

    ; ── EPILOGUE ──────────────────────────────
    leave             ; equivalent to: mov rsp, rbp / pop rbp
    ret               ; return to caller (pops return address)
```

### Why spill parameters?

The System V ABI (the calling convention used on Linux x86-64)
specifies that function arguments are passed in registers:
`rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`.

But S++ stores everything on the stack. So immediately after the
prologue, the codegen emits instructions to copy the registers
into their stack slots:

```nasm
; fn add(a: int, b: int) -> int
; Before fix: a and b are in rdi/rsi but [rbp-8] is uninitialized
; After fix:
    mov  qword [rbp - 8],  rdi   ; a = rdi
    mov  qword [rbp - 16], rsi   ; b = rsi
; Now all subsequent code can uniformly read from [rbp - N]
```

This was the bug that caused wrong output (1,1,15,1,0 instead of
42,1,15,120,0) — parameters were being read from uninitialized memory.

### IR to assembly translation

Each IR instruction type maps to a small fixed sequence of assembly:

**ASSIGN_LIT — load a literal:**
```
IR:    t0 = 42
ASM:   mov  qword [rbp - 8], 42
```

**ASSIGN_VAR — copy a variable:**
```
IR:    t1 = x
ASM:   mov  rax, qword [rbp - 16]   ; load x into rax
       mov  qword [rbp - 24], rax    ; store into t1
```
(cannot do memory-to-memory in x86, must go through a register)

**STORE — write back to named variable:**
```
IR:    x = t0
ASM:   mov  rax, qword [rbp - 8]    ; load t0
       mov  qword [rbp - 16], rax   ; store to x
```

**BINARY — arithmetic:**
```
IR:    t2 = t0 + t1
ASM:   mov  rax, qword [rbp - 8]    ; load t0
       add  rax, qword [rbp - 16]   ; add t1
       mov  qword [rbp - 24], rax   ; store result

IR:    t2 = t0 - t1
ASM:   mov  rax, qword [rbp - 8]
       sub  rax, qword [rbp - 16]
       mov  qword [rbp - 24], rax

IR:    t2 = t0 * t1
ASM:   mov  rax, qword [rbp - 8]
       imul rax, qword [rbp - 16]   ; signed multiply
       mov  qword [rbp - 24], rax

IR:    t2 = t0 / t1
ASM:   mov  rax, qword [rbp - 8]
       cqo                           ; sign-extend rax into rdx:rax
       idiv qword [rbp - 16]         ; quotient → rax, remainder → rdx
       mov  qword [rbp - 24], rax

IR:    t2 = t0 % t1
ASM:   mov  rax, qword [rbp - 8]
       cqo
       idiv qword [rbp - 16]
       mov  qword [rbp - 24], rdx    ; remainder is in rdx
```

**BINARY — comparisons:**
```
IR:    t3 = t0 < t1
ASM:   mov  rax, qword [rbp - 8]
       cmp  rax, qword [rbp - 16]   ; set CPU flags
       setl al                       ; al = 1 if less, 0 otherwise
       movzx rax, al                 ; zero-extend al to 64-bit rax
       mov  qword [rbp - 32], rax

; setl → less than
; setg → greater than
; setle → less or equal
; setge → greater or equal
; sete → equal
; setne → not equal
```

**BINARY — logical:**
```
IR:    t3 = t0 && t1
ASM:   mov  rax, qword [rbp - 8]
       and  rax, qword [rbp - 16]   ; bitwise AND (works for 0/1 bools)
       mov  qword [rbp - 32], rax

IR:    t3 = t0 || t1
ASM:   mov  rax, qword [rbp - 8]
       or   rax, qword [rbp - 16]
       mov  qword [rbp - 32], rax
```

**UNARY:**
```
IR:    t1 = -t0
ASM:   mov  rax, qword [rbp - 8]
       neg  rax
       mov  qword [rbp - 16], rax

IR:    t1 = !t0
ASM:   cmp  qword [rbp - 8], 0
       sete al                       ; 1 if was zero, 0 if was nonzero
       movzx rax, al
       mov  qword [rbp - 16], rax
```

**IFFALSE — conditional jump:**
```
IR:    iffalse t0 goto L1
ASM:   cmp  qword [rbp - 8], 0
       je   L1                       ; jump if equal to zero (false)
```

**GOTO — unconditional jump:**
```
IR:    goto L0
ASM:   jmp  L0
```

**LABEL:**
```
IR:    L0:
ASM:   L0:
```

**PARAM + CALL — function call:**
```
IR:    param t0
       param t1
       t2 = call add(2)
ASM:   mov  rdi, qword [rbp - 8]    ; arg 1 → rdi
       mov  rsi, qword [rbp - 16]   ; arg 2 → rsi
       call add
       mov  qword [rbp - 40], rax   ; store return value
```

**PRINT:**
```
IR:    print t0
ASM:   mov  rsi, qword [rbp - 8]   ; value to print
       lea  rdi, [rel fmt_int]      ; format string "%lld\n"
       xor  eax, eax               ; al=0: no xmm args
       call printf
```

**RETURN:**
```
IR:    return t0
ASM:   mov  rax, qword [rbp - 8]   ; return value goes in rax
       leave
       ret
```


### String literals in .data

Every unique string literal is **interned** — stored once in the `.data`
section with a generated label (`str0`, `str1`, ...). When the same
string appears multiple times, the same label is reused.

```nasm
section .data
    fmt_int   db "%lld", 10, 0
    fmt_float db "%f",   10, 0
    fmt_str   db "%s",   10, 0     ← new format for strings
    str0 db "S++", 0               ← interned string
    str1 db "Hello, World!", 0
    str2 db "col1", 9, "col2", 0   ← tab (ASCII 9) emitted as number
```

Escape sequences are resolved by the lexer and the codegen emits
non-printable characters as their decimal ASCII values:
```
"col1\tcol2"  →  db "col1", 9, "col2", 0
"line1\nline2" →  db "line1", 10, "line2", 0
```

**String variable IR → assembly:**
```
IR:   t0 = __str__  (op_str = "hello")
ASM:  lea  rax, [rel str0]      ; load address of string label
      mov  qword [rbp-8], rax   ; store pointer in stack slot
```
Strings are stored as **pointers** on the stack (8 bytes, just like int),
but they point into the `.data` section instead of holding a value.

**Print string vs print int:**
```
IR:   print t0

If t0 is string type:
ASM:  mov  rsi, qword [rbp-8]   ; pointer to string
      lea  rdi, [rel fmt_str]   ; "%s\n"
      xor  eax, eax
      call printf

If t0 is int/bool type:
ASM:  mov  rsi, qword [rbp-8]   ; integer value
      lea  rdi, [rel fmt_int]   ; "%lld\n"
      xor  eax, eax
      call printf
```
The codegen maintains a `typeMap` — a dictionary from temp/variable name
to type string — to know which format to use at each `print`.

### Output file structure

Every compiled S++ program produces this structure:

```nasm
section .data
    fmt_int   db "%lld", 10, 0    ; integer/bool format: "%lld\n"
    fmt_float db "%f",   10, 0    ; float format: "%f\n"
    fmt_str   db "%s",   10, 0    ; string format: "%s\n"
    str0 db "hello", 0            ; interned string literals (if any)

section .text
    global main                    ; entry point for linker
    extern printf                  ; use C library printf

; ── function 1 ─────────────────────────────────
add:
    push rbp
    mov  rbp, rsp
    sub  rsp, <frame size>
    ; spill params
    ; body instructions
    leave
    ret

; ── function 2 ─────────────────────────────────
main:
    push rbp
    mov  rbp, rsp
    sub  rsp, <frame size>
    ; body instructions
    xor  eax, eax
    leave
    ret
```

---

## End-to-End Walkthrough

Let's trace a complete program through all 5 phases:

```spp
fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() -> void {
    let x: int = add(3, 4);
    print(x);
}
```

### Phase 1 output — tokens

```
FN, IDENT(add), LPAREN, IDENT(a), COLON, TYPE_INT, COMMA,
IDENT(b), COLON, TYPE_INT, RPAREN, ARROW, TYPE_INT, LBRACE,
RETURN, IDENT(a), PLUS, IDENT(b), SEMICOLON, RBRACE,

FN, IDENT(main), LPAREN, RPAREN, ARROW, TYPE_VOID, LBRACE,
LET, IDENT(x), COLON, TYPE_INT, EQ,
IDENT(add), LPAREN, INT_LIT(3), COMMA, INT_LIT(4), RPAREN, SEMICOLON,
PRINT, LPAREN, IDENT(x), RPAREN, SEMICOLON,
RBRACE, EOF
```

### Phase 2 output — AST

```
Program
├── FnDecl(add) -> int
│   ├── Params: a:int, b:int
│   └── Body:
│       └── ReturnStmt
│           └── Binary(+)
│               ├── Var(a)
│               └── Var(b)
└── FnDecl(main) -> void
    └── Body:
        ├── LetStmt(x: int)
        │   └── Call(add)
        │       ├── IntLit(3)
        │       └── IntLit(4)
        └── PrintStmt
            └── Var(x)
```

### Phase 3 output — semantic check

```
Pass 1 — collect functions:
  fnTable[add]  = {return:int,  params:[int,int]}
  fnTable[main] = {return:void, params:[]}

Pass 2 — check add:
  scope[0] = {a:int, b:int}  (params pre-declared)
  ReturnStmt:
    checkExpr(Binary(+)):
      checkExpr(Var(a)) → int ✓
      checkExpr(Var(b)) → int ✓
      int + int → int ✓
    return int, currentReturnType=int ✓

Pass 2 — check main:
  scope[0] = {}
  LetStmt(x: int):
    checkExpr(Call(add, [IntLit(3), IntLit(4)])):
      add exists ✓
      2 args, expects 2 ✓
      arg1: int == param1: int ✓
      arg2: int == param2: int ✓
      returns int ✓
    int == int ✓ → declare x:int
  PrintStmt:
    checkExpr(Var(x)) → int ✓
    int != void ✓

Semantic Analysis: OK
```

### Phase 4 output — IR

```
=== fn add ===
  [begin]
  t0 = a
  t1 = b
  t2 = t0 + t1
  return t2
  [end]

=== fn main ===
  [begin]
  t3 = 3
  param t3
  t4 = 4
  param t4
  t5 = call add(2)
  x = t5
  t6 = x
  print t6
  [end]
```

### Phase 5 output — assembly

```nasm
section .data
    fmt_int   db "%lld", 10, 0
    fmt_float db "%f",   10, 0

section .text
    global main
    extern printf

add:
    push rbp
    mov  rbp, rsp
    sub  rsp, 32
    mov  qword [rbp - 8],  rdi     ; spill a
    mov  qword [rbp - 16], rsi     ; spill b
    mov  rax, qword [rbp - 8]      ; t0 = a
    mov  qword [rbp - 24], rax
    mov  rax, qword [rbp - 16]     ; t1 = b
    mov  qword [rbp - 32], rax     ; (wait, already used 24)
    mov  rax, qword [rbp - 24]     ; t2 = t0 + t1
    add  rax, qword [rbp - 32]
    mov  qword [rbp - 40], rax
    mov  rax, qword [rbp - 40]     ; return t2
    leave
    ret
    xor  eax, eax
    leave
    ret

main:
    push rbp
    mov  rbp, rsp
    sub  rsp, 64
    mov  qword [rbp - 8],  3       ; t3 = 3
    mov  qword [rbp - 16], 4       ; t4 = 4
    mov  rdi, qword [rbp - 8]      ; param t3 → rdi
    mov  rsi, qword [rbp - 16]     ; param t4 → rsi
    call add                        ; t5 = call add(2)
    mov  qword [rbp - 24], rax
    mov  rax, qword [rbp - 24]     ; x = t5
    mov  qword [rbp - 32], rax
    mov  rax, qword [rbp - 32]     ; t6 = x
    mov  qword [rbp - 40], rax
    mov  rsi, qword [rbp - 40]     ; print t6
    lea  rdi, [rel fmt_int]
    xor  eax, eax
    call printf
    xor  eax, eax
    leave
    ret
```

### Runtime output

```
7
```

---

## File Structure Reference

```
spp/
│
├── lexer.hpp          Token enum, Token struct, Lexer class declaration
├── lexer.cpp          Lexer implementation: character scanning, token building
│
├── ast.hpp            All AST node structs (Expr, Stmt, Block, FnDecl, Program)
│                      TypeKind enum, factory helper functions
│
├── parser.hpp         Parser class declaration, one method per grammar rule
├── parser.cpp         Recursive descent parser implementation
│
├── sema.hpp           FnSig struct, SemanticAnalyzer class declaration
├── sema.cpp           Two-pass semantic analysis, type checking, scope stack
│
├── ir.hpp             IROp enum, IRInstr struct, IRFunction, IRProgram
│                      printIR() pretty printer
├── irgen.hpp          IRGen class declaration
├── irgen.cpp          AST → IR lowering, temp/label allocation
│
├── codegen.hpp        CodeGen class declaration
├── codegen.cpp        IR → x86-64 NASM assembly, stack frame management
│
├── main.cpp           Compiler driver: runs all 5 phases, writes .asm file
├── Makefile           Build rules: 'make' builds compiler, 'make run' compiles spp
├── run.sh             One-command script: compile + assemble + link + run + cleanup
│
└── spp-vscode/        VS Code extension for syntax highlighting
    ├── package.json             Extension manifest
    ├── language-configuration.json  Brackets, comments, indentation rules
    ├── spp-icon-theme.json      File icon theme
    ├── icon.png                 Marketplace icon
    ├── icons/spp.svg            File explorer icon
    └── syntaxes/
        └── spp.tmLanguage.json  TextMate grammar (color rules)
```