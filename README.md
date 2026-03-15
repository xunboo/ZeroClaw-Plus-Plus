# ZeroClaw++

ZeroClaw++ is a C++17 port of the original Rust-based ZeroClaw autonomous agent framework. It provides a robust, extensible architecture for building and running LLM-powered agents with multi-provider support, sophisticated tool orchestration, and sandbox security.

## Features

- **Multi-Provider LLM Support**: Built-in integrations for OpenAI, Anthropic, Gemini, Ollama, Bedrock, OpenRouter, and more.
- **Extensive Tool Ecosystem**: Includes tools for filesystem operations, shell execution, web browsing, HTTP requests, git operations, cron scheduling, and hardware interactions.
- **Advanced Security Module**: Native sandbox auto-detection and enforcement using Landlock (Linux), Firejail, Bubblewrap, or Docker, ensuring safe tool execution. Emergency stop (estop) capabilities and OTP-authorized resumes.
- **Flexible Channels**: Connect your agent to various platforms including CLI, Telegram, Discord, Slack, Signal, Matrix, and more.
- **Memory & RAG**: Context-aware memory loading and vector-based document chunking/retrieval.
- **SkillForge**: A pipeline to scout, evaluate, and integrate new skills dynamically.

## Building from Source

ZeroClaw++ utilizes CMake for its build system and requires a C++17 compatible compiler (e.g., MSVC, GCC, Clang).

### Prerequisites
- CMake (3.15+)
- C++17 Compiler
- OpenSSL (Included in `include/openssl-3.5.5/`)

### Build Instructions (Windows / MSVC)

1. Open a Developer Command Prompt for Visual Studio.
2. Navigate to the project root directory.
3. Configure the project using CMake:
   ```cmd
   cmake -B build -S .
   ```
4. Build the executable:
   ```cmd
   cmake --build build -j 1
   ```

The compiled executable `zeroclaw.exe` will be located in the `build/` directory (or `build/Debug/` / `build/Release/` depending on the generator used).

## Project Structure

- `src/`: The core C++ source files.
  - `agent/`: Core agent loop, memory loading, and dispatching logic.
  - `auth/`: OAuth and profile management.
  - `channels/`: Integrations for various messaging platforms.
  - `http/`: Unified HTTP transport layer.
  - `providers/`: LLM API integrations.
  - `security/`: High-level security policies, sandbox implementations, and OTP validation.
  - `tools/`: Built-in tools executable by the agent.
  - `...`
- `include/`: Header-only libraries and dependencies (e.g., `cpp-httplib`, `nlohmann/json`, OpenSSL).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. It is intended for open-source, research, academic, and personal use.
