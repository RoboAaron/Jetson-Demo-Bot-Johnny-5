# Branch Consolidation Recommendation

**Date**: 2026-02-12  
**Goal**: Simplify branches and bring valuable work back into `main` before resuming coding.

---

## Current state

- **main** (origin): At merge commit `c74d336` — includes integration from `integration/main-nonspi-from-feature` (cascaded firmware, tuning GUI, docs/delivery, VescUart.zip, SPI_WIRING_GUIDE, etc.).
- All other branches share the same divergence point with main: **9112f3c** (old main before the integration merge).

---

## Branch summary

| Branch | Commits not in main | What it contains | Recommendation |
|--------|---------------------|------------------|----------------|
| **origin/claude/identify-prd-tasks-AIhMn** | 11 | TASKS.md, PBI-5 roadmap, Jetson/ESP32 ROS2 bridge, TeensyComms refactor, #VEL parser, .gitignore, `docs/delivery/5/tasks.md`, `firmware/` layout + patch | **Bring into main** (see below) |
| **origin/claude/implement-todo-item-iLhMy** | 3 | ROS2 packages (johnny5_bringup, johnny5_description, johnny5_gazebo, johnny5_sensor_fusion), URDF/Gazebo fixes, TODO.md | **Merge into main** (clean, no overlap) |
| **origin/claude/review-balance-code-Xgv6T** | 116 | Cascaded balance firmware + GUI history (regex, velocity loop, motor direction, etc.) | **Do not merge** — same lineage as feature/spi-migration; main already has the resulting code from the integration. Keep as reference or ignore. |
| **origin/feature/spi-migration** | 115 | Same balance work as above + SPI-specific docs/sketches we intentionally excluded | **Do not merge** — allowlisted content is already on main. Optional: delete after cleanup. |
| **origin/integration/main-nonspi-from-feature** | 0 | Already merged into main | **Delete** (local + remote) — done. |
| **origin/codex/create-documentation-for-setup_windows_dev.bat** | 0 | Already in main (PR #1) | **No action** |
| **origin/pbi1-task1-1** | 1 | "1-1 Setup Teensy project skeleton" | **Do not merge** — likely superseded by identify-prd-tasks or main structure. |
| **origin/pbi1-task1-2** | 41 | Old balance path: archive layout, tuning history, SOFTWARE_DESIGN_DOCUMENT, etc. | **Do not merge** — superseded by main (main has current cascaded firmware + docs). |
| **spi-conversion** (local only) | 43 | Same tip as pbi1-task1-2 | **Delete** — redundant. |

---

## Overlap and conflicts

- **identify-prd-tasks** vs **main**:
  - **Conflicts / different layout**: `tuning_code/robot_tuning_gui.py`, `tuning_code/teensy_comms.py`; firmware at `firmware/teensy_balance_cascaded/` on branch vs `teensy_balance_cascaded/` at repo root on main.
  - **Unique on branch (no conflict)**: `TASKS.md`, `docs/delivery/5/tasks.md`, `jetson/`, `esp32/`, `firmware/FIRMWARE_DESIGN.md`, `firmware/patches/`, `setup_teensy_dev.md`, `.gitignore` updates.
  - **Strategy**: Prefer **main’s** `tuning_code/` and root-level `teensy_balance_cascaded/`. From identify-prd-tasks, bring in only the additive pieces (TASKS.md, PBI-5 tasks, jetson/, esp32/, firmware design + patch, .gitignore, setup doc) via merge with conflict resolution or cherry-pick.

- **implement-todo-item** vs **main**: No path overlap; safe to merge.

---

## Recommended order of operations

1. **Update local main**
   - `git checkout main && git pull origin main`

2. **Merge implement-todo-item into main (easiest)**
   - Adds ROS2 packages and TODO.md with no structural conflict.
   ```bash
   git checkout main
   git pull origin main
   git merge origin/claude/implement-todo-item-iLhMy -m "Merge implement-todo-item: ROS2 packages, URDF/Gazebo, TODO"
   git push origin main
   ```

3. **Bring identify-prd-tasks content into main**
   - Option A — **Full merge, then resolve conflicts** (keep main’s tuning_code and root teensy layout; take branch’s new files and docs):
     ```bash
     git checkout main
     git merge origin/claude/identify-prd-tasks-AIhMn -m "Merge identify-prd-tasks: TASKS, PBI-5, Jetson/ESP2 bridge, firmware design"
     # Resolve conflicts: keep main's version for tuning_code/* and any firmware path that duplicates root teensy_balance_cascaded
     git add . && git commit && git push origin main
     ```
   - Option B — **Cherry-pick only non-conflicting commits** (e.g. TASKS.md, .gitignore, docs/delivery/5, jetson/, esp32/, firmware/FIRMWARE_DESIGN.md and firmware/patches) and then manually add any missing files from the branch. Slower but avoids large conflict resolution.

4. **Clean up branches (after main is updated)**
   - Delete merged / obsolete branches so only **main** (+ optional one active feature branch) remains in use:
   - **Delete (local + remote if applicable):**
     - `integration/main-nonspi-from-feature`
     - `spi-conversion` (local)
   - **Optional delete (or leave as reference):**
     - `feature/spi-migration`
     - `origin/claude/review-balance-code-Xgv6T`
     - `pbi1-task1-1`, `pbi1-task1-2` (and remotes)
     - `origin/claude/identify-prd-tasks-AIhMn` and `origin/claude/implement-todo-item-iLhMy` after their merges are done

5. **Going forward**
   - **main** = single source of truth.
   - Use short-lived feature branches off `main` for new work; merge back to `main` and delete the branch when done.
   - Avoid long-lived branches (e.g. feature/spi-migration) unless you explicitly want a parallel line.

---

## One-line summary

**Merge implement-todo-item into main, then merge identify-prd-tasks into main (resolving conflicts by keeping main’s tuning_code and root teensy layout). Delete or ignore the rest so only main (+ optional short-lived branches) remains.**
