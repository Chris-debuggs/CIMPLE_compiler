# Tests

CIMPLE oracle workflow:

1. Evaluator output is the specification.
2. Native backend output must match evaluator output byte-for-byte.

Run all `.cimp` tests in this folder:

```powershell
python .\run_tests.py
```

Useful options:

- `--cimple <path>`: use a specific `cimple` binary
- `--strict-native`: fail if LLVM backend is unavailable
- `--stop-on-fail`: stop at first failure
- `--pattern "*.cimp"`: custom test glob

Example:

```powershell
python .\run_tests.py --strict-native --stop-on-fail
```
