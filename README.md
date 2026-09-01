# Building

To build the project for local development, you can use the provided `shell.nix` file to set up a Nix shell environment with all necessary dependencies. Follow these steps:

```bash
# Enter the nix-shell environment
nix-shell

# Run CMake to configure the build system
cmake .. -DCMAKE_INSTALL_PREFIX=../install_test -DCMAKE_INSTALL_LIBDIR=lib64

# Compile and install
make -j$(nproc) install
```

# License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details
