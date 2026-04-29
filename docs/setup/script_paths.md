# Script Path Conventions

## Default Paths

The setup and bundle scripts in this repository use portable default paths that work on any fresh Linux/Jetson installation:

| Purpose | Default Path |
|---------|--------------|
| Jetson bundle | `~/j5_bundle` |
| ROS 2 workspace | `~/ros2_ws` |
| LiDAR workspace | `~/ld_ws` |

These defaults are intentional — they ensure scripts work out-of-the-box on a clean Jetson or development machine without requiring a specific directory structure.

## Using Custom Paths

If your local machine uses a different layout (e.g., `~/Projects/robotics/...`), pass the custom path as a command-line argument instead of editing the script defaults.

### Bundle Scripts

```bash
# Build bundle to custom location
./build_j5_bundle.sh ~/Projects/robotics/j5_bundle

# Verify bundle at custom location
./verify_bundle.sh ~/Projects/robotics/j5_bundle

# Update bundle at custom location
./scripts/update_bundle_jetson_setup.sh ~/Projects/robotics/j5_bundle
```

### LiDAR Scripts

The LiDAR setup scripts currently don't accept path arguments. If you need a custom workspace location, either:

1. Create a symlink: `ln -s ~/Projects/robotics/ld_ws ~/ld_ws`
2. Or edit locally but **do not commit** the path changes

### ROS 2 Workspace

For ROS 2 workspace paths embedded in generated systemd units or install scripts, the bundle scripts accept the workspace path via environment or argument. Check each script's usage comment at the top.

## Why Not Commit Custom Paths?

Hardcoding machine-specific paths like `~/Projects/robotics/...` into the repo:

- Breaks scripts on fresh Jetson deployments
- Breaks scripts for other contributors
- Makes the repo less portable

The argument-based approach keeps defaults portable while allowing per-machine customization without touching version-controlled files.

## Shell Aliases (Optional)

For convenience on your dev machine, add aliases to `~/.bashrc`:

```bash
alias build-bundle='~/Projects/robotics/Jetson-Demo-Bot-Johnny-5/build_j5_bundle.sh ~/Projects/robotics/j5_bundle'
alias verify-bundle='~/Projects/robotics/Jetson-Demo-Bot-Johnny-5/verify_bundle.sh ~/Projects/robotics/j5_bundle'
```

This gives you short commands with your preferred paths, without modifying the scripts.
