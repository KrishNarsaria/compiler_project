# End-to-End Compiler: C++ Subset to RISC-V (RV64IMFD) Assembly

An end-to-end compiler written in **C++** using **Flex** (lexical analysis) and **Bison / YACC** (syntax and grammar parsing) that compiles a C++ subset language directly into **RISC-V (RV64IMFD)** 64-bit target assembly code.

---

## 1. Required Simulator & Execution Instructions

### Simulator Required
* **RARS 1.6** (RISC-V Assembler and Runtime Simulator), 64-bit mode.
* Download / Repository: https://github.com/TheThirdOne/rars/releases
* A verified copy is included directly in this repository as `rars.jar`.
* Prerequisites: Java Runtime Environment (JRE / JDK 8 or newer).

### How to Execute Generated Assembly
To execute the generated assembly binary with RARS:

```bash
# Basic execution
java -jar rars.jar rv64 nc me <output.s>

# Execution with standard input redirection
java -jar rars.jar rv64 nc me <output.s> < input.txt

# Execution with command-line arguments (passed to main as argc, argv)
java -jar rars.jar rv64 nc me <output.s> pa arg1 arg2 10 20
```

#### Simulator Flags Explained:
* `rv64`: Selects the 64-bit RISC-V architectural execution mode.
* `nc`: No copyright notice (clean console output).
* `me`: Simulator error and status messages are directed to standard error, preserving clean standard output.
* `pa`: Passes the remaining trailing tokens as command-line arguments (`argc`, `argv`) to the program's `main()` function.

---

## 2. Feature Implementation Status

Below is the exact checklist corresponding to the submission requirements and Google Form specifications.

### Implementation Language
* [ ] C
* [x] **C++**
* [ ] Python
* [ ] Java
* [ ] Other

---

### Basic Features Checklist

All basic features required by the specification are fully implemented and verified with automated test cases:

| Status | Feature | Verification / Notes |
|:------:|:--------|:----------------------|
| [x] | **All arithmetic and logical operator** | `+`, `-`, `*`, `/`, `%`, `&`, `\|`, `^`, `<<`, `>>`, `&&`, `\|\|`, `!`, `~`, `==`, `!=`, `<`, `>`, `<=`, `>=`, ternary `?:`, compound assignments (`+=`, `-=`, etc.), prefix/postfix `++`/`--` (Tested in `t01_basics`, `t06_operators_full`, `t23_precedence`, `t37_precedence_torture`) |
| [x] | **if-else** | Nested conditional branches, short-circuit boolean evaluation, dangling-else resolution (Tested in `t01_basics`, `t07_control_flow`, `t27_control_flow_deep`) |
| [x] | **for loop** | Standard 3-clause for loops, empty clauses, declarations in loop init (Tested in `t01_basics`, `t07_control_flow`, `t27_control_flow_deep`) |
| [x] | **while loop** | Pre-tested loops with expression conditions (Tested in `t01_basics`, `t07_control_flow`) |
| [x] | **do while loop** | Post-tested do-while loops with condition testing at loop tail (Tested in `t07_control_flow`, `t27_control_flow_deep`) |
| [x] | **switch cases** | Multi-way branching, integer/char/constant case labels, `default` label, case fallthrough, `break` (Tested in `t07_control_flow`, `t27_control_flow_deep`) |
| [x] | **array (integer and char)** | Single- and multidimensional arrays, string literals as char arrays, element indexing, array decay to pointer (Tested in `t01_basics`, `t08_arrays_pointers`, `t28_strings_chars`) |
| [x] | **pointers** | Address-of (`&`), dereference (`*`), pointer arithmetic with automatic type scaling, arrow operator (`->`) (Tested in `t01_basics`, `t08_arrays_pointers`, `t26_pointer_deep`) |
| [x] | **structure** | Struct declaration, layout calculation with byte alignment, member access (`.`), nested structures, struct assignments and pass-by-value (Tested in `t01_basics`, `t09_struct_union_enum_typedef`, `t30_struct_deep`) |
| [x] | **printf and scanf** | Variadic formatted I/O via runtime library, supporting `%d`, `%i`, `%u`, `%x`, `%X`, `%o`, `%c`, `%s`, `%p`, `%f`, `%e`, `%g`, width, precision, and length modifiers (Tested in `t01_basics`, `t02_advanced`, `t10_functions_overload`) |
| [x] | **function call with arguments** | Argument passing in RV64 ABI stack slots, register returns (`a0`/`fa0`), type conversions on call (Tested across all `t*.cpp` tests) |
| [x] | **goto, break and continue** | Labeled statements, unconditional jumps (`goto`), structured loop exits (`break`, `continue`) (Tested in `t07_control_flow`, `t11_static_goto`, `t27_control_flow_deep`) |
| [x] | **static keywords** | Static local variables retained in `.data` across invocations, static functions with internal scope (Tested in `t11_static_goto`, `t36_scope_linkage`) |

---

### Advanced Features Checklist

The table below clearly indicates which features are marked as contracted in the Google Form submission versus unmarked. Notice that the compiler implementation goes beyond the requirements and implements all unmarked features as well:

| Form Mark | Implementation Status | Feature | Description & Test Coverage |
|:---------:|:---------------------:|:--------|:----------------------------|
| **[x]** | **Implemented** | **Recursive Function call** | Full support for direct and mutual recursion with independent stack frames (Tested in `t01_basics`, `t02_advanced`, `t31_recursion_deep`) |
| **[ ]** | **Implemented** | **Class and object** | Class definitions, constructors, destructors, methods, explicit/implicit `this` pointer (Tested in `t03_classes`, `t12_classes`) |
| **[ ]** | **Implemented** | **Inheritence** | Single inheritance, base object embedding, base constructor chaining, base destructor sequencing (Tested in `t13_inheritance`) |
| **[ ]** | **Implemented** | **Function call with variable arguments** | `va_list`, `va_start`, `va_arg`, `va_end` supporting user-defined variadic functions (Tested in `t04_funcptr_lambda_variadic`, `t17_variadic`, `t34_varargs_deep`) |
| **[x]** | **Implemented** | **Dynamic memory allocation** | Heap allocation via `malloc()`, `calloc()`, `free()`, `new`, `new[]`, `delete`, `delete[]` backed by `sbrk` (Tested in `t02_advanced`, `t14_dynamic_memory`, `t32_dynamic_deep`) |
| **[ ]** | **Implemented** | **Lambda function** | C++11 lambdas with capture modes `[]`, `[=]`, `[&]`, and explicit captures `[x, &y]`, trailing return types (Tested in `t04_funcptr_lambda_variadic`, `t20_lambdas`, `t33_lambda_deep`) |
| **[ ]** | **Implemented** | **Function pointer** | Function pointers, pointer typedefs, array of function pointers, indirect invocation (Tested in `t04_funcptr_lambda_variadic`, `t16_funcptr`) |
| **[x]** | **Implemented** | **Command line input** | Command-line arguments forwarded from startup runtime (`_start`) into `int main(int argc, char **argv)` (Tested in `t02_advanced`, `t38_cmdline`) |
| **[ ]** | **Implemented** | **public, private and protected keywords** | Strict compiler enforcement of access control in classes and inheritance (Tested in `t03_classes`, `t12_classes`, `e04_access`, `e24_access_deep`) |
| **[x]** | **Implemented** | **typedef** | Aliasing primitive types, pointer types, structures, arrays, and function pointers (Tested in `t02_advanced`, `t09_struct_union_enum_typedef`) |
| **[x]** | **Implemented** | **Reference** | C++ reference types (`T&`), reference parameters (pass-by-reference), and reference return values (return lvalues) (Tested in `t02_advanced`, `t15_references`) |
| **[x]** | **Implemented** | **enum, union** | Named and anonymous enums with custom initializers, union overlapping storage layout (Tested in `t02_advanced`, `t09_struct_union_enum_typedef`) |
| **[ ]** | **Implemented** | **File manipulation** | `fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell`, `fgets`, `fputs`, `fprintf`, `fscanf`, `feof`, `rewind` (Tested in `t05_files`, `t18_files`) |
| **[x]** | **Implemented** | **until loop** | `until (cond) stmt;` and `do stmt until (cond);` loops (Tested in `t02_advanced`, `t07_control_flow`, `t27_control_flow_deep`) |
| **[x]** | **Implemented** | **Multi-level pointers** | Double, triple, and n-level indirection (`int**`, `char***`), address-of and dereference propagation (Tested in `t02_advanced`, `t08_arrays_pointers`, `t26_pointer_deep`) |
| **[ ]** | **Implemented** | **Multi-dimensional arrays** | 2D, 3D, and n-dimensional arrays with row-major layout and multi-subscript indexing (Tested in `t02_advanced`, `t08_arrays_pointers`, `t35_multidim_deep`) |
| **[x]** | **Implemented** | **Other: Function overloading** | Type-based function signature matching, overload resolution based on argument conversion cost (Tested in `t02_advanced`, `t10_functions_overload`) |

---

## 3. Project Structure & Deliverables

```
compiler_project/
├── Makefile            # Main compilation recipe
├── makefile            # Lowercase alias for make
├── run.sh              # Test harness script to compile and run all test cases
├── rars.jar            # RISC-V Assembler and Runtime Simulator 1.6
├── README.md           # Comprehensive compiler documentation & evaluation
├── src/                # Compiler source code (Flex, Bison, C++)
│   ├── ast.h           # AST node structures, Type definitions, Symbol table interfaces
│   ├── ast.cpp         # Type checking, struct/class layout, constant folding, error logging
│   ├── lexer.l         # Flex lexical specification and lexical error diagnostics
│   ├── parser.y        # Bison LALR(1) grammar, AST building, and CLI driver
│   ├── codegen.cpp     # Semantic analyzer and Three-Address Code (TAC/IR) generator
│   ├── backend.h       # RISC-V RV64 backend interface
│   └── backend.cpp     # RISC-V (RV64IMFD) code generator emitting target assembly
├── runtime/            # Runtime C library
│   └── runtime.c       # Standard C library implementation (I/O, strings, memory, math)
└── test/               # Comprehensive test suite (38 execution tests + 29 error tests)
    ├── t01_basics.cpp .. t38_cmdline.cpp   # Positive execution tests
    ├── t*.out, t*.in, t*.args             # Reference outputs, stdin feeds, cmdline args
    └── e01_lexical.cpp .. e29_flow_errors.cpp # Negative diagnostic error tests
```

### Commenting Compliance (Functions > 30 LOC)
Per the assignment instructions: *"If LOC for any of the function definition is more than 30 lines then please write comment for that function."*
* Every function definition throughout `src/ast.cpp`, `src/backend.cpp`, `src/codegen.cpp`, `src/lexer.l`, `src/parser.y`, and `runtime/runtime.c` exceeding 30 lines contains an explicit function header comment documenting its purpose, inputs, outputs, and algorithmic steps.

---

## 4. Build and Run Instructions

### Prerequisites
* `g++` (supporting C++14 or newer)
* `make`
* `flex`
* `bison`
* `java` (JRE 8 or newer, for running the RARS simulator)

### Building the Compiler
Run `make` at the repository root:

```bash
make
```

This compiles:
1. Stage 0 compiler (`build/rvcc0`) without embedded runtime.
2. Compiles `runtime/runtime.c` using the stage 0 compiler into RISC-V assembly (`build/runtime.s`).
3. Embeds the assembly runtime into the final executable `rvcc`.

To remove all build artifacts:
```bash
make clean
```

### Compiling Programs with `rvcc`
```bash
# Compile source to default output (output/<source_name>.s)
./rvcc prog.cpp

# Compile source to specific assembly file
./rvcc prog.cpp out.s

# Emit Three-Address Code intermediate representation as well
./rvcc -ir prog.cpp out.s    # generates out.s and out.ir

# Print Intermediate Representation directly to console
./rvcc -p prog.cpp
```

### Running the Test Suite
The automated test runner executes the compiler against all test cases in `test/`, verifies compiler exit codes, validates produced IR against expectations, runs the generated assembly on the RARS simulator, and diffs the output against expected reference files:

```bash
./run.sh ./rvcc
```

---

## 5. Evaluation, Test Results, and Known Issues

### Test Suite Execution Results
The test suite was executed end-to-end using `./run.sh ./rvcc`:

```
============================================================
Passed: 67   Failed: 0   Total: 67
============================================================
```

#### Test Breakdown:
* **38 Positive Execution Tests (`t01` – `t38`)**:
  * 100% compiled successfully without diagnostic errors.
  * Generated RISC-V 64-bit assembly executed on the RARS simulator.
  * Runtime output matched the reference `.out` files character-for-character.
  * Verified command-line arguments handling (`t38_cmdline.args` -> `argc`/`argv`).
  * Verified standard input handling (`t01_basics.in`, `t19_streams.in`).
* **29 Negative Error Tests (`e01` – `e29`)**:
  * Tested lexical errors (unterminated strings, invalid hex/octal escapes, illegal characters).
  * Tested syntax errors (malformed declarations, mismatched parentheses, invalid tokens).
  * Tested semantic errors (undeclared identifiers, duplicate member declarations, private access violations, invalid pointer conversions, array dimension errors, return type mismatches).
  * Verified compiler properly halts with non-zero exit status and **does not** emit assembly files.
  * Verified exact error diagnostics match the expected messages and correct line numbers.

### Evaluation: How Good Is It?
* **Correctness**: The compiler strictly enforces language semantics. Every program that compiles runs accurately on standard RISC-V hardware simulation.
* **Diagnostics**: Error messages report precise line numbers and descriptive explanations matching GCC/Clang conventions.
* **Optimization & Cleanliness**: Constant expressions (arithmetic, bitwise, boolean logic, ternary) are folded at compile time. Array sizes, enumerator values, and global initializers are evaluated directly into constants.
* **ABI Compliance**: RV64 calling conventions are respected; functions pass parameters via consecutive 8-byte stack slots, return integer/pointer values in `a0`, and floating-point values in `fa0`.

### Known Limitations, Errors, or Problems Observed
1. **Simulator System Calls (RARS Ecalls)**:
   * System calls rely on RARS environment ecalls (`sbrk`, file write, file read, file open, file close).
   * In RARS batch mode, standard error output is combined with standard output unless the `me` flag is passed.
2. **Dynamic Memory Reclamation**:
   * The runtime heap uses a bump allocator over the `sbrk` service (`ecall 9`). While `malloc()`, `calloc()`, and `realloc()` function correctly, `free()` is currently a no-op that does not reclaim or coalesce fragmented blocks.
3. **C++ Language Scope Exclusions**:
   * C++ exceptions (`try`, `catch`, `throw`) are not supported.
   * C++ templates and namespaces are not supported.
   * `virtual` dispatch via vtables is not implemented (single inheritance uses static binding).
   * Multiple inheritance is not supported.
   * Block-exit RAII destructor calls are not invoked when breaking out of blocks via raw `goto`.
4. **File Operations**:
   * The simulator supports file modes `"r"`, `"w"`, and `"a"`. File removal (`remove`) is unsupported by the RARS environment.
