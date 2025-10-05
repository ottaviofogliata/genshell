# LLM Sidecar (Gemma via llama.cpp)

GenShell ships with an optional helper binary, `gemma_cli`, that runs a local Gemma model using llama.cpp. The shell must remain safe and responsive when the sidecar is unavailable; integration happens through a narrow shim that generates command suggestions and requires explicit user confirmation (`TAB`) before execution.

## Operational Flow
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

## Building the Helper
1. Initialise the submodule when first enabling the assistant:
   ```bash
   git submodule update --init --recursive
   ```
2. Build llama.cpp static libraries using the platform helper:
   ```bash
   ./build_mac_complete.sh   # Metal-enabled macOS
   # or
   ./build_pc_complete.sh    # CPU-only Linux/Windows
   ```
3. Re-run the regular helper with the `all` target if you skipped step 2:
   ```bash
   ./build_mac.sh all
   ./build_pc.sh all
   ```

## Preparing Gemma Models
1. From `deps/llama.cpp`, install Python requirements:
   ```bash
   python3 -m pip install -r requirements.txt
   ```
2. Download Gemma weights (use `huggingface_hub` or any preferred tooling).
3. Convert to GGUF and optionally quantise:
   ```bash
   python3 convert_hf_to_gguf.py --model <repo> --outfile models/gemma.gguf
   ./llama-quantize models/gemma.gguf models/gemma-q4.gguf q4_0
   ```

Store GGUF files under `models/` (ignored by git). Launch the helper directly:
```bash
./bin/gemma_cli models/gemma-q4.gguf "List a few automation tasks."
```

## Safety & Sampling Defaults
- Sampling defaults are configured in `src/llm/gemma_cli.c`; review the long comment documenting temperature, top-k, and other knobs before changing behaviour.
- High-risk commands (e.g., `rm -rf /`) are suppressed via a logit-bias block. Extend the list when exposing new workflows or running on shared systems.
- Maintain documentation of new flags in `gemma_sampling_config` and surface them through CLI parameters if relevant.

## Failure Modes
- The shell never blocks if the model is missing. Handle absence of `bin/gemma_cli` gracefully and continue executing handwritten commands.
- Network access is not required; keep inference fully local to honour repository portability expectations.
- Log inference errors through the shim but avoid crashing the shell. Treat inference suggestions as advisory.

For implementation notes and sampling defaults, review `src/llm` and `include/gemma_sampling_config.h` (when available).
