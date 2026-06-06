# S++ Language

Syntax highlighting for the S++ programming language.

## Features
- Keywords: `fn`, `let`, `if`, `else`, `while`, `return`
- Types: `int`, `float`, `bool`, `void`
- Literals, operators, comments

## Usage
Files with `.spp` extension are automatically highlighted.

## Example
\`\`\`spp
fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() -> void {
    let x: int = add(3, 4);
    print(x);
}
\`\`\`