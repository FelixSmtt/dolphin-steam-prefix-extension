```bash
# Enter the nix-shell environment
nix-shell

# Create a build directory and enter it
cd build
rm -rf *

# Configure using lib64 for NixOS
cmake .. -DCMAKE_INSTALL_PREFIX=../install_test -DREQUIRE_OPENCLOUD_RESOURCES=OFF -DCMAKE_INSTALL_LIBDIR=lib64

# Compile and install
make -j$(nproc) install

# Correct the environment paths (pointing to lib64)
export QT_PLUGIN_PATH="$PWD/install_test/lib64/plugins:$QT_PLUGIN_PATH"
export QT_DEBUG_PLUGINS=1
# export XDG_DATA_DIRS="$PWD/../install_test/share:$XDG_DATA_DIRS"

# Force kill any running dolphin daemon instances so it reloads plugins
kquitapp6 dolphin 2>/dev/null

# Launch Dolphin in the foreground to catch any console logs/errors
dolphin
```
