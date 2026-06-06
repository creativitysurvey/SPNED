## Micro-architectural Fidelity in Context Switching
To rigorously evaluate the true cost of entering a WebAssembly Sandbox without the distortion of standard `volatile` busy-loops (which fail to simulate micro-architectural state destruction), our benchmark utilizes raw **x86-64 Inline Assembly**:
1. **Pipeline Serialization**: We inject a `cpuid` instruction to force a complete drain of the superscalar execution pipeline, mimicking the heavy security barrier of SFI transitions.
2. **Register Spilling**: We explicitly clobber the general-purpose registers (`r8`-`r15`) and apply a `memory` clobber, forcing the compiler to emit authentic `push/pop` stack instructions. This physically replicates the Caller/Callee context saving required when bridging the host and the Wasm VM.
