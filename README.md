# SSS Project

Source-level control-flow integrity (CFI) for C, built for the SSS course.

The code lives in [`starter/`](starter/):

- `libguard/`: the Guard runtime (function-pointer checks and a shadow stack)
- `rewriter/`: Python source rewriter that inserts the `GUARD_*` instrumentation
- `testcases/`: benign and attack test programs

## Build and test

```sh
cd starter
make        # build libguard and the test binaries
make test   # run the test suite
```

See [`starter/README.md`](starter/README.md) for details.
