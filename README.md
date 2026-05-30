# S++ Programming Language

<div align="center">

![S++ Logo](spp-vscode/icon.jpg)

**A simple, statically-typed, compiled programming language**

*Compiles directly to x86-64 native assembly*

</div>

---

## Table of Contents

1. [What is S++?](#what-is-s)
2. [Getting Started](#getting-started)
3. [Language Reference](#language-reference)
   - [Types](#types)
   - [Variables](#variables)
   - [Operators](#operators)
   - [Control Flow](#control-flow)
   - [Functions](#functions)
   - [Print](#print)
   - [Comments](#comments)
4. [Full Example Programs](#full-example-programs)
5. [Compiler Usage](#compiler-usage)
6. [Error Messages](#error-messages)

---

## What is S++?

S++ is a statically-typed, compiled programming language that produces native x86-64 assembly code. It is designed to be simple and easy to learn while demonstrating all the core phases of a real compiler.

Every S++ program is:
- **Lexed** — source text broken into tokens
- **Parsed** — tokens structured into an AST
- **Type-checked** — types verified at compile time
- **Compiled** — translated to real x86-64 assembly and run natively

---

## Getting Started

### Prerequisites

```bash
# Linux / WSL
sudo apt install g++ nasm gcc
```

### Build the compiler

```bash
git clone <repo>
cd spp
```

### Write your first program

Create `hello.spp`:

```
fn main() -> void {
    let x: int = 42;
    print(x);
}
```

### Run it

```bash
./run.sh hello.spp
# output: 42
```

---

## Language Reference

---

### Types

S++ has 4 built-in types:

| Type    | Description           | Example values        |
|---------|-----------------------|-----------------------|
| `int`   | 64-bit integer        | `0`, `42`, `-10`      |
| `float` | 64-bit floating point | `3.14`, `0.5`, `-1.0` |
| `bool`  | Boolean               | `true`, `false`       |
| `void`  | No value (functions only) | —                 |

---

### Variables

Variables are declared with `let`, followed by a name, type annotation, and initial value. Every variable **must** be initialized at declaration.

**Syntax:**
```
let <name>: <type> = <expression>;
```

**Examples:**
```
let age: int = 25;
let pi: float = 3.14159;
let isReady: bool = true;
```

**Reassigning a variable:**
```
let x: int = 10;
x = x + 1;       // x is now 11
```

> **Rules:**
> - You cannot use a variable before declaring it
> - You cannot declare the same variable twice in the same scope
> - The type of the value must match the declared type

---

### Operators

#### Arithmetic Operators

| Operator | Description    | Example         | Result |
|----------|----------------|-----------------|--------|
| `+`      | Addition       | `5 + 3`         | `8`    |
| `-`      | Subtraction    | `10 - 4`        | `6`    |
| `*`      | Multiplication | `3 * 7`         | `21`   |
| `/`      | Division       | `20 / 4`        | `5`    |
| `%`      | Modulo         | `10 % 3`        | `1`    |

> `%` only works on `int`. Both operands must be the same type.

#### Comparison Operators

All comparison operators return a `bool`.

| Operator | Description           | Example    | Result  |
|----------|-----------------------|------------|---------|
| `==`     | Equal                 | `5 == 5`   | `true`  |
| `!=`     | Not equal             | `5 != 3`   | `true`  |
| `<`      | Less than             | `3 < 7`    | `true`  |
| `>`      | Greater than          | `7 > 3`    | `true`  |
| `<=`     | Less than or equal    | `5 <= 5`   | `true`  |
| `>=`     | Greater than or equal | `6 >= 10`  | `false` |

#### Logical Operators

| Operator | Description | Example            | Result  |
|----------|-------------|--------------------|---------|
| `&&`     | AND         | `true && false`    | `false` |
| `\|\|`   | OR          | `true \|\| false`  | `true`  |
| `!`      | NOT         | `!true`            | `false` |

> Logical operators only work on `bool` values.

#### Operator Precedence (high to low)

| Level | Operators         |
|-------|-------------------|
| 1     | `!` `-` (unary)   |
| 2     | `*` `/` `%`       |
| 3     | `+` `-`           |
| 4     | `<` `>` `<=` `>=` |
| 5     | `==` `!=`         |
| 6     | `&&`              |
| 7     | `\|\|`            |

Use parentheses to override precedence:
```
let x: int = (2 + 3) * 4;   // 20, not 14
```

---

### Control Flow

#### If / Else

```
if (<condition>) {
    // runs when condition is true
} else {
    // runs when condition is false (optional)
}
```

**Examples:**
```
let score: int = 85;

if (score >= 90) {
    print(1);    // A grade
} else {
    print(0);    // not A grade
}
```

Without else:
```
let x: int = 10;
if (x > 0) {
    print(x);
}
```

Nested if/else:
```
let n: int = 72;

if (n >= 90) {
    print(3);    // A
} else {
    if (n >= 60) {
        print(2);    // B
    } else {
        print(1);    // C
    }
}
```

> **Rule:** The condition must be of type `bool`.
> `if (x)` where `x` is an `int` is a **compile error**.

---

#### While Loop

```
while (<condition>) {
    // body — runs as long as condition is true
}
```

**Example — count from 1 to 5:**
```
let i: int = 1;
while (i <= 5) {
    print(i);
    i = i + 1;
}
// prints: 1 2 3 4 5
```

**Example — sum of first N numbers:**
```
let n: int = 10;
let sum: int = 0;
let i: int = 1;

while (i <= n) {
    sum = sum + i;
    i = i + 1;
}
print(sum);    // 55
```

> **Rule:** The condition must be of type `bool`.

---

### Functions

Functions are declared with `fn`, have typed parameters, and a return type.

**Syntax:**
```
fn <name>(<param>: <type>, ...) -> <returnType> {
    // body
    return <value>;
}
```

**Examples:**

Simple function:
```
fn add(a: int, b: int) -> int {
    return a + b;
}
```

Calling a function:
```
fn main() -> void {
    let result: int = add(10, 20);
    print(result);    // 30
}
```

Function with no parameters:
```
fn greet() -> void {
    print(1);
}
```

Recursive function:
```
fn factorial(n: int) -> int {
    if (n <= 1) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}
```

Boolean returning function:
```
fn isEven(n: int) -> bool {
    return n % 2 == 0;
}
```

> **Rules:**
> - Functions must be declared at the top level (not inside other functions)
> - The return type of the `return` expression must match the declared `->` type
> - `void` functions do not need a `return` statement
> - Functions can call each other and themselves (recursion)
> - Functions can be called before they are defined (forward calls work)

---

### Print

`print` outputs a value followed by a newline.

```
print(<expression>);
```

**Examples:**
```
print(42);
print(x);
print(a + b);
print(isEven(4));
```

> Works with `int`, `float`, and `bool`. Cannot print `void`.

---

### Comments

Only single-line comments are supported:

```
// This is a comment
let x: int = 10;   // inline comment
```

---

## Full Example Programs

### Example 1 — FizzBuzz
```
fn main() -> void {
    let i: int = 1;
    while (i <= 20) {
        let fizz: bool = i % 3 == 0;
        let buzz: bool = i % 5 == 0;
        if (fizz && buzz) {
            print(0);    // FizzBuzz
        } else {
            if (fizz) {
                print(1);    // Fizz
            } else {
                if (buzz) {
                    print(2);    // Buzz
                } else {
                    print(i);
                }
            }
        }
        i = i + 1;
    }
}
```

### Example 2 — Fibonacci
```
fn fib(n: int) -> int {
    if (n <= 1) {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

fn main() -> void {
    let i: int = 0;
    while (i < 10) {
        print(fib(i));
        i = i + 1;
    }
}
```

### Example 3 — Power function
```
fn power(base: int, exp: int) -> int {
    let result: int = 1;
    while (exp > 0) {
        result = result * base;
        exp = exp - 1;
    }
    return result;
}

fn main() -> void {
    print(power(2, 10));    // 1024
    print(power(3, 5));     // 243
}
```

### Example 4 — GCD
```
fn gcd(a: int, b: int) -> int {
    while (b != 0) {
        let temp: int = b;
        b = a % b;
        a = temp;
    }
    return a;
}

fn main() -> void {
    print(gcd(48, 18));    // 6
    print(gcd(100, 75));   // 25
}
```

---

## Compiler Usage

```bash
# Build compiler
./build.sh

# Compile and run a .spp file
./run.sh program.spp

# Manual step-by-step
./spp program.spp             # produces program.asm
nasm -f elf64 program.asm -o program.o
gcc program.o -o program -no-pie
./program
```

---

## Error Messages

S++ gives clear error messages for every mistake:

| Error | Cause | Fix |
|-------|-------|-----|
| `undeclared variable 'x'` | Using `x` before `let x` | Declare the variable first |
| `'x' already declared in this scope` | Two `let x` in same block | Use different name or remove duplicate |
| `call to undeclared function 'foo'` | Calling a function that doesn't exist | Check spelling or declare the function |
| `'add' expects 2 argument(s), got 3` | Wrong number of arguments | Match the function signature |
| `cannot assign float to 'x' of type int` | Type mismatch in assignment | Use matching types |
| `if condition must be bool, got int` | Non-bool in if/while | Use a comparison: `if (x > 0)` not `if (x)` |
| `return type mismatch: expected int, got bool` | Wrong return type | Return the correct type |