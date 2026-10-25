# Retro: Redesign v2

Cycle: `/grill-with-docs` -> `/to-spec` -> `/to-tickets` -> `/implement-spec` -> `/code-review` ->
`/retro`. Branch `feat/redesign-v2`, 17 tickets, 21 commits from `a5b75fa9`.

This document is about the **environment** (repo, tooling, steering, provider), not the feature.
Ordered by severity.

## What worked

- Grilling before code settled the decisions that would otherwise have been rewritten: the absolute
  240x320 layout, fixed type roles, A8 icons, the Settings hub, the test seams and the budget. The
  spec and tickets were then mostly transcription.
- The spec -> tickets -> per-ticket commit chain is what made the 429 recovery possible. The
  subagent had committed tickets 06-14 and left a named WIP for 15; the resume could continue from
  the commit, not from a chat log.
- The 240x320 simulator with the F1-F9 dev navigation and the committed captures was the cheapest
  verification loop this repo has had. Nine screens were checked without a flash; the harness found
  real layout bugs (the keyboard's screen parent, the caption glyphs, the field's theme background).
- Splitting the pure logic from the LVGL code kept the seams testable: seven suites, 36 cases, 153
  assertions, compiled standalone while the full `build-tests` target stayed unreachable.
- The final direct session (after the provider limits) finished 15-17 because each ticket was small,
  the WIP diff was legible, and the code review could be done against a fixed commit range.

## What cost time

### 1. The provider 429s made a long implementation non-resumable by design

The first `/implement-spec` ran as one long subagent. It completed tickets 06-14, started 15, and
then failed three times with `rate_limit_error` (429), including after a 15-minute wait. The work
itself was fine; the session was not. The user had to reconstruct the resume from the last commit
(`34ef487a`) and three WIP paths before the final direct session could continue.

The per-ticket commits are the reason nothing was lost. The missing piece is a **state file**: last
commit, WIP files, next ticket, one line of context. The user's resume message did that job by hand;
the next run should not need a human to remember it.

Action: for any cycle longer than a few tickets, write `docs/mobile/<feature>-state.md` and update it
in the same commit as each ticket. Do not run the whole implementation in one long subagent; use the
direct session or short, checkpointed subagents. A 20-minute checkpoint that commits and writes the
state file is cheaper than a resume.

### 2. The simulator cannot build in this worktree

The worktree's `Libraries/FreeRTOS-Kernel` is the dirty upstream checkout (no CMakeLists.txt); the
simulator needs the fork that the main checkout has. The build was made to work by a source copy
outside the repo with symlinked Libraries and the main checkout's FreeRTOS-Kernel. That copy is
invisible in the repo, so the next session will rediscover the problem.

The simulator also needed the mbedtls nested submodules (`framework`, `tf-psa-crypto`,
`mldsa-native`), the mock-driver include path for the generated devicetree, and a POSIX guard for the
display-idle service that the ESP build never compiles. Each was a small fix, but only the simulator
found them.

Action: a `Buildscripts/sim.sh` (or `tt sim`) that creates the out-of-tree source copy, points
`Libraries/FreeRTOS-Kernel` at the correct fork, initializes the mbedtls nested submodules, sets the
240x320 profile and the screenshot path. Add a simulator build to CI: it is the only job that would
have caught the POSIX guard and would keep the visual harness alive.

### 3. The Python toolchain is undocumented, and one shim damaged the host Python

The repo's build scripts call `python`, which does not exist on macOS. The workaround grew into a
venv, a `python` shim, `lark`, `pyyaml`, `jinja2` and `jsonschema`, and a 3.12 venv because the
Homebrew 3.14 interpreter hung. The hang was self-inflicted: `cat >` through a symlink overwrote the
Homebrew `python3`/framework binary with the shim; the wrapper then recursed through the venv's
symlink back to itself. `brew reinstall python@3.14` restored the host (3.14.5 -> 3.14.9) and the
symlink had to be recreated by hand.

Two lessons:

- never write through a symlink; remove it first or write a new file in a dedicated directory, and
  check with `file` and `ls -l` before running the result;
- the toolchain setup must be reproducible. The existing `ios-gestures` retro already flagged the
  unreachable local test target; this session paid the cost a second time.

Action: `requirements-dev.txt` plus a `tt env`/`tt test` command that creates a 3.12 venv and a
**wrapper script** named `python` on a dedicated `PATH` entry. Document the venv and the wrapper in
the developer README. The wrapper is a two-line shell file, not a symlink.

### 4. The mechanical standards have no check

The code review found two hard standards defects that a machine should have caught: a new source
file without the SPDX header, and the temporary `StackProbe.cpp` still in the tree after its own
comment said "remove before the final commit". The `.clang-format` file exists but no CI job or hook
runs it. The font registry bug (a three-slot array indexed by the new roles) was found by a visual
tofu glyph, not by a test.

Action: one `tt check` (or a CI job) that runs, on changed files:

- `clang-format --dry-run --Werror`;
- first-line SPDX check for new `.c/.cpp/.h/.hpp`;
- a grep for `Temporary`, `StackProbe` and `Remove this file` in source;
- a compile-time or test assertion tying the text-font array size to `FONT_SIZE_COUNT`;
- the simulator build from the previous action.

The feature follow-ups (statusbar Wi-Fi mask, +96 B static DRAM, runtime heap/high-water) are
known and accepted; they are not environment fixes.

### 5. Navigation is partly outside the repo

The mockups live on a separate branch/worktree, and the spec gives an absolute machine path. The
icon sources were copied into the repo, but the PNG/SPEC truth is still external. The workspace also
has no `graphify-out`, so the "graphify before grep" rule was a no-op for the whole session; the
exploration used two `explore` subagents and `grep` instead. The `grill-with-docs` skill exists in
`~/.agents/skills` but was not in the Skill tool catalog, so the first step quietly became
`grilling` and the `domain-modeling`/ADR half was skipped (the spec carries the glossary and ADR
candidates inline).

Action: add a `docs/mobile/design-v2/README.md` with the branch, commit, clone path and the
fallback (the committed captures and icon sources). Either run `graphify update .` as the first step
of an implement cycle, or remove the rule from the brief. Expose `grill-with-docs` and the other
installed skills in the tool catalog, or document the file path in `AGENTS.md`.

### 6. Tool economy: size, build and capture waits

- `idf.py size` re-runs the build; `python -m esp_idf_size build/Tactility.map` does not. The
  previous retro said this; it is still the right command.
- Deleting a source file leaves the CMake source glob stale; `idf.py build` fails with "missing and
  no known rule", and `idf.py reconfigure` fixes it. The build note belongs next to the build
  command.
- The simulator captures need more time than the obvious 14 s. The action list schedules the
  screenshot on the LVGL task; the first run killed five simulators just before their shot. A fixed
  30 s wait, or a helper that waits for the file, would have saved five restarts.
- The final ESP link took several minutes. A `tt build` wrapper that prints the phase and uses the
  map would keep the agent from assuming a hang.

### 7. Information access: the device and the mock fixtures

The heap/high-water measurements need a flash; the rule forbids one without the owner's OK. The
report carries the probe snippet and the task list, but the numbers are still pending. The
machine has a serial relay and a prior `/tmp/serial-crash.log`; a read-only serial monitor would at
least capture the boot heap without a new flash.

The simulator has no audio service, so the Settings hub capture has no Sound row, and its mock Wi-Fi
driver reports the radio off to the statusbar and the hub while the Wi-Fi app's card shows the mock
fixture connected. Those are fixture weaknesses, not product defects; they make the visual evidence
ambiguous.

Action: add a mock audio device (or hide audio consistently) and make the mock Wi-Fi state read the
same for every consumer. Add a read-only serial/monitor command for the owner's probe build.

## What to keep

- The 240x320 simulator profile, the F1-F9 navigation, the committed `design-checks` captures and
  their README. This is the best verification loop in the repo.
- The spec/ticket/per-ticket-commit chain. The 429 resume proved its value.
- The pure seams as separate standalone doctest suites: they survive the unreachable full test
  target.
- The two-axis review. The standards axis found the SPDX/probe; the spec axis found the statusbar
  Wi-Fi and the static-DRAM budget miss.
- `python -m esp_idf_size` and the `Tactility.map`, not `idf.py size`.

## Actions, in order

1. A state file for long cycles (`docs/mobile/<feature>-state.md`: last commit, WIP, next ticket),
   written in each ticket commit; no single long subagent for a whole spec.
2. `tt sim`/`Buildscripts/sim.sh`: out-of-tree simulator copy, fork FreeRTOS-Kernel, mbedtls nested
   submodules, 240x320 profile, screenshot; add the simulator build to CI.
3. `requirements-dev.txt` + `tt env`/`tt test`: 3.12 venv, a **wrapper** `python` (never a symlink),
   lark/pyyaml/jinja2/jsonschema; document it.
4. `tt check`: clang-format, SPDX header, temporary-marker grep, font-registry static assert,
   simulator build.
5. Navigation: a design-v2 README with branch/commit/clone path; `graphify update .` before grep or
   remove that rule; expose `grill-with-docs` (and the other installed skills) in the tool catalog;
   use `impeccable`/`frontend-design` for the visual review pass.
6. Simulator fixtures: mock audio and consistent mock Wi-Fi state; capture wait 30 s or wait for the
   file.
7. A read-only serial/monitor command for the owner's probe build, so the boot heap and high-water
   numbers do not need a new flash decision.
8. A build note: `idf.py reconfigure` after adding or removing a source file; `tt build` prints the
   phase and uses `esp_idf_size`.
