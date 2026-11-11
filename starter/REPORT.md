# Guard — Report

## Group members and contributions

| Member | Worked on |
|--------|-----------|
| TODO   | TODO      |
| TODO   | TODO      |
| TODO   | TODO      |
| TODO   | TODO      |

---

## Part 1: Runtime library and manual instrumentation

### Why tail calls break the shadow stack

A *tail call* is a call that is the very last thing a function does, e.g. `return b(x);`. With optimisation enabled,
gcc compiles it as a jump instead of a call ("sibling call optimisation"):

```
normal call                       tail call
  a:  ...                           a:  ...
      call b                            (tear down a's frame)
      ret        ; back to main         jmp b    ; no new return address pushed
```

After the `jmp`, `b` reuses `a`'s return address, so when `b` returns it goes straight back to `a`'s caller (`main`).
`a` never executes its own return. That saves a stack frame, but it breaks the assumption a shadow stack relies on:
every `GUARD_ENTER()` push is matched by exactly one `GUARD_RETURN` pop, in the same frame.

Example, with `main` calling `a` and `a` tail-calling `b`. `R_start` is `main`'s return address, `R_main` the address
in `main` right after the call to `a` (the top of the shadow stack is on the right):

| step                                                  | shadow stack                 |
|-------------------------------------------------------|------------------------------|
| `main`: `GUARD_ENTER()`                               | `[R_start]`                  |
| `a`: `GUARD_ENTER()`                                  | `[R_start, R_main]`          |
| `a`: `jmp b` (tail call, `a`'s check never runs)      | `[R_start, R_main]`          |
| `b`: `GUARD_ENTER()` (its return address is `R_main`) | `[R_start, R_main, R_main]`  |
| `b`: `GUARD_RETURN`: `R_main == R_main`, pop          | `[R_start, R_main]`          |
| `main`: `GUARD_RETURN`: `R_start != R_main`           | **violation**                |

Every check compares with the *top* of the shadow stack, which is only the function's own entry if every entry pushed
after it was popped again. `b`'s check passes, but `a`'s entry is never popped and stays on top of `main`'s entry. When
`main` returns, it finds `a`'s stale `R_main` instead of its own `R_start` and Guard reports a violation: a false
positive in a benign program. In general every function that is left through a tail call leaves an entry behind for
its caller to trip over, and in a long-running loop these stale entries pile up until the shadow stack overflows.

`-fno-optimize-sibling-calls` forbids this optimisation, so every call is a real `call`/`ret` pair and every push has
its pop.

---

## Part 2: guardcc

### Why `guard_seal()` is not done in a constructor

TODO

### Limitations

1. TODO
2. TODO

---

## Part 3: Real-world robustness

TODO

---

## Part 4: Evaluation

### Security analysis: attacks Guard does not stop

1. **Calling a different valid target.** TODO
2. **Data-only attacks.** TODO
3. **Arbitrary write targeting the shadow stack itself.** The shadow stack's bookkeeping (`base`, `top`, `limit` in
   `struct shadow_stack`) is stored in ordinary writable memory (`.bss`), and the shadow stack region itself stays
   mapped read/write so that pushes and pops work. An attacker with an arbitrary write primitive can therefore:
   - overwrite the saved return address on the real stack *and* the matching entry on the shadow stack with the same
     value, so the check on return passes; or
   - modify `top` so that it points to an attacker-controlled slot holding the forged return address.

   The guard pages only catch linear overflows out of the region, not targeted writes into it. Hardware shadow stacks
   (Intel CET) avoid this by making the shadow stack pages unwritable by ordinary stores.

The `bypass` test case demonstrates attack TODO.

### Performance

TODO: benchmark description, configurations (no Guard, forward-edge only, backward-edge only, both), median of ≥10
runs per configuration.
