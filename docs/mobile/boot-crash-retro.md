# Retro: Boot crash

Cycle: `/grill-with-docs` -> `/to-spec` -> `/to-tickets` -> `/implement-spec` -> `/code-review` ->
`/retro`. Branch `feat/mobile-home`, commits `f1e48070` and `62e03377`.

## What worked

- The QR was enough to classify the crash (stack overflow) even though it carried only panic frames.
  Knowing the class before touching the device ruled out the memory-fault hypotheses early.
- The serial capture the owner already had named the task. That one line replaced a day of guessing.
- Measuring `entry a1` per function turned a vague "the Home is heavy" into a table, and showed the
  real problem was a unit, not an amount.

## What cost time

- **The units bug.** `AppStackConfig::depth` was documented "in words", but ESP-IDF defines
  `portSTACK_TYPE` as a byte. The launcher asked for 3072 and got 3 KB, not 12 KB. Every budget in
  the footprint doc was computed in the wrong unit, so the stack looked 4x larger than it was.
- **`lvgl_run_on_task()` lies.** The name promises the LVGL task; the body locks and calls inline on
  the caller. This is why an app with a deliberately small stack ends up building widgets.
- **The first stack-size reading was wrong** because `objdump` prints `entry a1` in hex and the
  first parser only read decimal. It produced a plausible, wrong "maximum frame is 240 bytes".
  A measurement that contradicts the failure mode is a bug in the measurement.
- **This binutils cannot decode `lv_obj_create`** (empty instruction text), so the LVGL part of the
  chain is not measurable from the linked ELF. That is what forced the stack doubling instead of a
  computed value.

## What to keep

- Rule of thumb confirmed: when a task crashes on a path that "worked before", check the units and
  the true stack size before touching the code.
- The high-water probe (documented in the tickets, kept out of the tree) is the cheapest way to
  turn an estimate into a number. It should be the first thing added to any future stack work.

## Actions

- BC-07: make `lvgl_run_on_task()` mean what it says, or rename it. Every app is building its
  widgets on its own stack today.
- BC-08: `lockScreenShow()` builds widgets on the FreeRTOS timer daemon (5 KB, shared by every
  software timer). Same failure class, not yet observed because it needs a lock timeout.
- BC-09: put the panic reason in the QR, so the next crash screenshot names its own task.
- When a device with PSRAM reports an app stack, state the unit next to the number in the manifest.
  Two lines of comment would have saved this cycle.
