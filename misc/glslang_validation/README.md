# glslang token-copy validation

Standalone regression checks for GH-123319. These include the actual vendored
`PpContext.h`, rather than a copied implementation. No SCons invocation is needed.
Requires a C++17 compiler supporting AddressSanitizer and UndefinedBehaviorSanitizer.

```sh
misc/glslang_validation/run.sh /tmp/godot-glslang-validation
```

The bounds test compares copying against the previous `snprintf` behavior for
1,283 string lengths (empty through oversized), checks NUL termination, untouched
buffer tails, adjacent canaries and token metadata, and exercises non-NUL-terminated
source allocations, explicit prefix lengths, and `SIZE_MAX` truncation.

For preprocessing and parse/link integration, provide a previously built Godot
`libmodule_glslang` archive from this checkout (same compiler/ABI, built-in glslang
and without LTO). The script rebuilds all translation units including the changed
header and uses the archive only for unchanged objects. An example on Linux:

```sh
GLSLANG_ARCHIVE="$PWD/bin/obj/modules/libmodule_glslang.linuxbsd.editor.x86_64.a" \
  misc/glslang_validation/run.sh /tmp/godot-glslang-validation
```

Runtime tests verify macro replay, punctuation, identifier/operator token pasting,
stringification, and a maximum-length identifier. Each checks the preprocessed
expansion and parses/links both original and expanded shaders. These are compiler
checks, not rendered GPU tests. The runtime executable is not sanitizer-instrumented;
the standalone token-copy bounds executable is.

If LeakSanitizer cannot operate because the host runs processes under ptrace,
prefix the command with `ASAN_OPTIONS=detect_leaks=0`. This disables leak checking
only; address and undefined-behavior checks remain enabled. Runtime tests are
explicitly skipped when `GLSLANG_ARCHIVE` is unset.
