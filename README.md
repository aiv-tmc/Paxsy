# Paxsy

**Paxsy** is a statically‑typed, compiled systems programming language with manual memory management, lightweight processes, and a modern toolchain.

Paxsy is designed for systems programming, embedded development, high‑performance computing, and cross‑platform application development. It compiles to native code for ARM AArch64 and x86‑64, with support for Android APK packages. The language combines the familiarity of C‑like syntax with modern features such as pattern matching, deferred execution (`defer`), built‑in error handling (`try`/`catch`), compile‑time evaluation (`comptime`), and safe concurrency through message‑passing processes (`proc`).

Paxsy gives you low‑level control without sacrificing developer productivity. Its syntax is familiar to C programmers, but it adds modern abstractions that make writing safe, concurrent, and high‑performance code more enjoyable. The toolchain is designed for simplicity and reproducibility – one TOML file configures everything from compiler flags to dependencies. Whether you are writing a kernel driver, a game engine, or a cloud service, Paxsy provides the performance and flexibility you need, backed by comprehensive documentation and an open community.

---

## Requirements

To build or install Paxsy from source, you need:

- **Git** – for cloning the repository (>= 2.0)
- **GCC** – a C compiler supporting C11 (>= 4.8)
- **Make** – to run the build system (>= 3.81)
- **Binutils** – `as`, `ar`, and `ld` for assembling and linking (>= 2.20)

On most Linux distributions these can be installed via the system package manager (e.g., `build-essential` on Debian/Ubuntu, `base-devel` on Arch Linux).

---

## Installation

There are two recommended ways to install Paxsy: a one‑line script, or manual build from source.

### Quick install (using curl)

Run the following command in your terminal:

```bash
curl -sSf https://paxsy.org/install.sh | sh
```

This script will:

- Detect your operating system and package manager (apt, dnf, pacman, apk) and attempt to install any missing build dependencies.
- Clone the Paxsy source code from GitHub (default branch: `main`).
- Build the compiler using `make`.
- Install the `paxsy` binary to `/usr/local/bin` (or the directory specified by `INSTALL_PATH`).

You can customise the installation by setting environment variables before running the script:

| Variable              | Purpose                                           | Default              |
|-----------------------|---------------------------------------------------|----------------------|
| `INSTALL_PATH`        | Destination for the `paxsy` binary                | `/usr/local/bin`     |
| `PAXSY_LIBRARY_DIR`   | Where to install libraries (if any)               | `/usr/local/lib/paxsy` |
| `PAXSY_SRC_DIR`       | Temporary directory for cloning and building      | `/tmp/paxsy-build`   |
| `BRANCH`              | Git branch to build                               | `main`               |
| `PAXSY_VERSION`       | Git tag or commit hash (overrides `BRANCH`)       | (unset)              |

For example, to install a specific version tag:

```bash
curl -sSf https://paxsy.org/install.sh | PAXSY_VERSION=v0.1.0 sh
```

You may also pass `--branch` or `--version` directly to the script:

```bash
curl -sSf https://paxsy.org/install.sh | sh -s -- --version v0.1.0
```

> **Note:** If you install to a system directory like `/usr/local/bin`, you may need `sudo` (the script will prompt if required). If you do not have write permissions, the script automatically falls back to `~/.local/bin` – make sure that directory is in your `PATH`.

### Manual build from source

If you prefer to build manually, follow these steps:

1. Clone the repository (or download a release archive):

   ```bash
   git clone https://github.com/aiv-tmc/paxsy.git
   cd paxsy
   ```

2. (Optional) Check out a specific branch or tag:

   ```bash
   git checkout <branch-or-tag>
   ```

3. Build the compiler:

   ```bash
   make
   ```

   This produces the `paxsy` executable in the project root.

4. (Optional) Install it to a directory of your choice:

   ```bash
   sudo install -D -m 755 paxsy /usr/local/bin/paxsy
   ```

   or copy it manually:

   ```bash
   cp paxsy ~/.local/bin/
   ```

---

## Usage

After installation, you can invoke the compiler with:

```bash
paxsy [options]
```

For a full list of options, run:

```bash
paxsy --help
```

---

## Documentation

Comprehensive documentation, including the language reference, standard library guide, and tutorials, is available at the official website:

[https://github.com/aiv-tmc/paxsy/wiki](https://github.com/aiv-tmc/paxsy/wiki)

If you find any inconsistencies or missing information, please [open an issue](https://github.com/aiv-tmc/paxsy/issues) on GitHub.

---

## Building from Source (Detailed)

The project uses a straightforward `Makefile` with the following structure:

```
CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -I. -I./src
OBJDIR   := obj
SRCS     := $(shell find src -name '*.c')   # all C files in src/
COMMON_SRCS := $(filter-out src/main.c src/libmain.c, $(SRCS))
```

The build produces the `paxsy` executable by compiling all source files except `src/libmain.c` (which is reserved for a possible static library). Object files are placed in the `obj/` directory mirroring the source tree.

### Available make targets

- **`all`** (default) – builds the `paxsy` binary.
- **`clean`** – removes the `obj/` directory and the `paxsy` binary.

### Customising the build

You can override the following variables when invoking `make`:

| Variable      | Purpose                           | Default           |
|---------------|-----------------------------------|-------------------|
| `CC`          | C compiler command                | `gcc`             |
| `CFLAGS`      | Compiler flags                    | `-std=c11 -Wall ...` |
| `BUILD_LIB`   | Set to `0` to skip library build  | `1` (enabled)     |

For example, to build with Clang and enable debug symbols:

```bash
make CC=clang CFLAGS="-std=c11 -g -O0"
```

---

## License

This project is licensed under the **Mozilla Public License 2.0**. See the [LICENSE](LICENSE) file for full details.

---

## Contributing

Contributions are welcome! Whether you want to report a bug, suggest a feature, or submit a pull request, please follow these guidelines:

1. Fork the repository on GitHub.
2. Create a new branch for your feature or fix (`git checkout -b my-feature`).
3. Make your changes, ensuring they follow the project's coding style (consistent with existing code).
4. Run `make` to ensure the project compiles without warnings.
5. Add or update tests if applicable.
6. Commit and push your branch.
7. Open a pull request against the `main` branch.

For major changes, please open an issue first to discuss your proposal. We also appreciate contributions to the documentation and examples.

---

## Authors

Paxsy is developed by the Paxsy team and contributors. See the [AUTHORS](AUTHORS) file for a full list of contributors.

---

## Links

- **GitHub repository**: [https://github.com/aiv-tmc/paxsy](https://github.com/aiv-tmc/paxsy)
- **Issue tracker**: [https://github.com/aiv-tmc/paxsy/issues](https://github.com/aiv-tmc/paxsy/issues)
- **Official website**: [https://paxsy.org](https://paxsy.org)
- **Documentation**: [https://github.com/aiv-tmc/paxsy/wiki](https://github.com/aiv-tmc/paxsy/wiki)
