# CS 537 Project 3: wsh Shell  

## Overview
This project implements a simplified Unix-like shell called **wsh**, supporting:

- Single commands (built-ins + external)
- Environment variable expansion (`$VAR`)
- Built-in commands:
  - `exit`
  - `cd`
  - `env`
- Searching and executing external programs via `PATH`
- Arbitrary-length pipelines (`a | b | c | d`)
- Batch and interactive modes

All required functionality is implemented.  
Final status: **All tests passed**.

---

## Implementation Details

### Command Parsing
The provided parser (`parser.c/h`) is used to construct:

- A `command_line` containing one or more pipelines  
- Each pipeline contains several commands  
- Each command contains an argument vector (argv)  

Variable expansion uses:
```c
char *get_variable(const char *var);
```

---

## Design Decisions & Issues Faced

### Exit Status Tracking
One subtle issue was with `last_exit` not being reset between commands. If a command failed (e.g., unknown command), `last_exit` would be set to 1 and persist into the next command's execution even if that command succeeded. The fix was to reset `last_exit = 0` at the start of each pipeline iteration, so each command is evaluated independently.

### Built-in Commands in Pipelines
Built-in commands (`cd`, `env`, `exit`) had to be handled both in the single-command path and inside pipeline children. Since pipeline commands run in forked child processes, `cd` inside a pipeline does not affect the parent shell's working directory — this is expected and consistent with how real shells behave.

### PATH Resolution
External commands without an absolute path are resolved by searching each directory in the `PATH` environment variable, splitting on `:` and using `access()` to check executability. If no match is found, an error is printed and `last_exit` is set to 1.

### Pipe Setup
For a pipeline of N commands, N-1 pipes are created upfront. Each child process redirects its stdin/stdout using `dup2` and closes all pipe file descriptors before calling `execv`. The parent closes all pipe ends after forking and waits for all children to finish.

### Memory Management
Care was taken to free all dynamically allocated memory — `cmd_line`, the parsed `command_line` structure, and intermediate buffers like `path_copy` and `dir_path` — to avoid leaks across iterations of the main loop.
