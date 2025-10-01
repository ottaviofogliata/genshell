# GenShell Native C Architecture: Ground-Up and Pure CLI

GenShell is a native shell application built Ground-Up in C. This approach ensures maximum performance and minimal overhead, supporting the Open Core business model.

The architecture is now designed for a Pure CLI experience, focusing on efficiency and speed, where the AI output is injected directly into the standard terminal input/output flow.

**We will actively explore new modes of interactions** such as the idea of a Visual Context Injector Module that captures and interprets the user's physical gestures via video camera to dynamically enrich the LLM prompt, or a simple double enter to accept then execute command.


## Modular Architecture and AI Flow

| Module               | Primary Responsibility                                                                     | Language |
| -------------------- | ------------------------------------------------------------------------------------------ | -------- |
| Shell Core (Kernel)  | REPL (Read-Eval-Print-Loop), fork()/execve() management, Essential Built-ins (cd, export). | C        |
| Context Engine       | Real-time data gathering (CWD, history, stderr) and structured LLM prompt formatting.      | C        |
| LLM Runner           | LLM inference management (binding to llama.cpp or Core ML).                                | C/C++    |

## Total Command Coverage (100% Compatibility)

GenShell achieves 100% coverage of all possible commands (sh, bash, zsh) despite being a Ground-Up shell, through a strategy of selective execution and delegation.

### Selective Execution Principle

- **Internal Commands (Built-ins):** GenShell directly executes only the essential Built-ins (cd, pwd, export, etc.) implemented in C in a lean manner and adhering to the POSIX standard.
- **External Commands/Scripts (Bash/Sh/Zsh):** For all more complex commands and scripts (.sh, .zsh, etc.), GenShell delegates execution to the correct interpreter specified in the Shebang (#!/bin/bash or #!/bin/sh) or to the binary found in the $PATH.
- **Mechanism:** GenShell uses C system calls (fork() and execve()) to launch the native system interpreter binary, ensuring total compatibility with any existing command or script.

## User Experience: Operational Flow and LLM Contract (Pure CLI Mode)

The flow is optimized for traditional terminal speed:

```mermaid
graph TD
    A["User Input"] --> B{Classifier};
    B -- "Natural Language" --> C[Prompt Structuring];
    C --> D[Local Inference];
    D --> E[JSON Output];
    E --> F[CLI Visualization];
    F --> G["User Accepts (TAB)"];
    G --> H["Execute Command"];
    B -- "Shell Command" --> H;
```

## Extensibility and Community Add-ons (The Ecosystem)

To give GenShell the scope of a large Open Source project, the Core will be extensible via Dynamically Loadable Modules (DLMs).

### Extensibility Strategy: Dynamic C Modules

GenShell exposes an API for DLMs, allowing the community to write C code that is loaded upon shell startup.
