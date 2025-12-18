# BASIC Interpreter in C

This project implements a small BASIC interpreter written in ANSI C. It supports basic commands and functionalities commonly found in BASIC programming, including variable handling, control flow, and a command-line interface.

## Features

- Supports integer variables from A-Z.
- Commands:
  - `LET` for variable assignment.
  - `PRINT` for outputting values.
  - `GOTO` for jumping to line numbers.
  - `IF-THEN` for conditional execution.
  - `END` to terminate the program.
- Command-line interface (CLI) with functionalities:
  - `LOAD` to load BASIC programs from files.
  - `SAVE` to save the current program to a file.
  - `LIST` to display the current program.
  - `RUN` to execute the loaded BASIC program.

## Project Structure

```
basic-interpreter-c
├── src
│   └── basic.c         # Implementation of the BASIC interpreter
├── examples
│   └── demo.bas        # Example BASIC program
├── Makefile            # Build instructions
├── .gitignore          # Files to ignore in Git
└── README.md           # Project documentation
```

## Compilation

To compile the BASIC interpreter, navigate to the project directory and run:

```
make
```

This will generate the executable for the BASIC interpreter.

## Usage

After compiling, you can run the interpreter with:

```
./basic_interpreter
```

### Commands

- **LOAD**: Load a BASIC program from a file.
- **SAVE**: Save the current program to a file.
- **LIST**: Display the current program.
- **RUN**: Execute the loaded BASIC program.

## Example

An example BASIC program can be found in `examples/demo.bas`. You can load this program into the interpreter to see how it works.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.