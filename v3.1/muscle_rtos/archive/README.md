# Archive - Superseded Files

These files are **no longer used** and have been archived for reference only.

## Superseded by `app_main_v1.c`

The canonical FreeRTOS application entry point is now `../app_main_v1.c`.

### Archived Files

| File | Description |
|------|-------------|
| `app_main.c` | Original app_main, replaced by v1 |
| `app_main_simple.c` | Simplified test version |
| `app_main_test.c` | Test harness version |
| `motion_task.c/h` | Old motion task implementation |
| `rpmsg_motion_rx.c/h` | Old RPMSG motion receiver |

## Do Not Use

These files are kept for historical reference. All new development should use `app_main_v1.c` and the modules in `motion_runtime/`.
