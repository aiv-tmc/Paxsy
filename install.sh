#!/bin/bash
# Paxsy Bootstrap Installer
# Official installation script from https://paxsy.org

set -eu
set -o pipefail

# Default values
INSTALL_PATH="${INSTALL_PATH:-/usr/local/bin}"
PAXSY_LIBRARY_DIR="${PAXSY_LIBRARY_DIR:-/usr/local/lib/paxsy}"
PAXSY_SRC_DIR="${PAXSY_SRC_DIR:-/tmp/paxsy-build}"
REPO_URL="https://github.com/aiv-tmc/paxsy.git"
BRANCH="${BRANCH:-main}"
PAXSY_VERSION="${PAXSY_VERSION:-}"          # tag, commit or release
DRY_RUN=0
UNINSTALL=0
MANIFEST_FILE=""

# ANSI colors – disabled if NO_COLOR is set
if [ -n "${NO_COLOR:-}" ]; then
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
else
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'
fi

# Helper functions
print_info() {
    printf "${BLUE}==>${NC} %s\n" "$*"
}

print_success() {
    printf "${GREEN}==>${NC} %s\n" "$*"
}

print_error() {
    printf "${RED}==> Error:${NC} %s\n" "$*" >&2
}

print_warning() {
    printf "${YELLOW}==> Warning:${NC} %s\n" "$*" >&2
}

# Cleanup temporary directory on exit or interrupt
cleanup() {
    if [ -n "${PAXSY_SRC_DIR:-}" ] && [ -d "$PAXSY_SRC_DIR" ] && [ "$DRY_RUN" -eq 0 ]; then
        print_info "Cleaning up build directory: $PAXSY_SRC_DIR"
        rm -rf "$PAXSY_SRC_DIR"
    fi
}
trap cleanup EXIT INT TERM

# Compute SHA256 checksum of a file (if sha256sum is available)
compute_sha256() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d' ' -f1
    else
        echo ""
    fi
}

# Write manifest file (list of installed files with their checksums)
create_manifest() {
    if [ "$DRY_RUN" -eq 1 ]; then
        return
    fi
    local manifest_dir
    manifest_dir="$(dirname "$MANIFEST_FILE")"
    mkdir -p "$manifest_dir"
    > "$MANIFEST_FILE"
}

add_to_manifest() {
    local file="$1"
    if [ "$DRY_RUN" -eq 1 ]; then
        return
    fi
    local hash
    hash="$(compute_sha256 "$file")"
    echo "$file $hash" >> "$MANIFEST_FILE"
}

# Check if we can write to the given directory
can_write_to() {
    local dir="$1"
    if [ ! -d "$dir" ]; then
        # Try to create it (with -p) and check result
        mkdir -p "$dir" 2>/dev/null || return 1
    fi
    touch "$dir/.write_test" 2>/dev/null && rm -f "$dir/.write_test"
}

# Suggest using ~/.local/bin if we lack permissions
suggest_local_bin() {
    local local_bin="${HOME}/.local/bin"
    print_warning "Cannot write to $INSTALL_PATH (permission denied)."
    if can_write_to "$local_bin"; then
        print_info "You have write permissions to $local_bin."
        echo "Would you like to install there instead? (y/N) "
        read -r answer
        if [[ "$answer" =~ ^[Yy]$ ]]; then
            INSTALL_PATH="$local_bin"
            PAXSY_LIBRARY_DIR="${HOME}/.local/lib/paxsy"
            MANIFEST_FILE="${PAXSY_LIBRARY_DIR}/.manifest"
            print_info "Installation path changed to $INSTALL_PATH"
            return 0
        fi
    fi
    print_error "Cannot proceed without write permissions. Please run with sudo or set INSTALL_PATH to a writable directory."
    exit 1
}

# Check if a git ref (tag or branch) exists on remote
git_ref_exists() {
    local ref="$1"
    # Check if it's a tag (exact match) or branch (heads)
    git ls-remote --exit-code --heads --tags "$REPO_URL" "$ref" >/dev/null 2>&1
}

# Check dependencies (print recommendations, but don't auto-install)
check_dependencies() {
    print_info "Checking for required tools..."
    local missing=()
    for cmd in git gcc make as ar; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing+=("$cmd")
        fi
    done
    # Optional: curl/wget
    for cmd in curl wget; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            # Not critical, just warning
            print_warning "Optional tool '$cmd' not found (used for some features)."
        fi
    done

    if [ ${#missing[@]} -eq 0 ]; then
        print_success "All required tools are available."
        return
    fi

    print_warning "Missing required tools: ${missing[*]}"
    echo "Please install them using your system's package manager:"
    if command -v pacman >/dev/null 2>&1; then
        echo "  sudo pacman -S git base-devel binutils"
    elif command -v apt-get >/dev/null 2>&1; then
        echo "  sudo apt-get install git build-essential binutils"
    elif command -v dnf >/dev/null 2>&1; then
        echo "  sudo dnf install git gcc make binutils"
    elif command -v apk >/dev/null 2>&1; then
        echo "  sudo apk add git gcc make binutils"
    else
        echo "  - git, gcc, make, binutils (as, ar, ld)"
    fi
    print_error "Please install the missing tools and rerun this script."
    exit 1
}

# Fetch source code (git clone/update)
fetch_source() {
    local ref="${PAXSY_VERSION:-$BRANCH}"
    print_info "Fetching Paxsy source from $REPO_URL (ref: $ref)..."

    if [ "$DRY_RUN" -eq 1 ]; then
        echo "  [dry-run] Would clone $REPO_URL into $PAXSY_SRC_DIR (ref $ref)"
        return
    fi

    if [ -d "$PAXSY_SRC_DIR/.git" ]; then
        print_info "Source directory exists. Updating..."
        (cd "$PAXSY_SRC_DIR" && git fetch --all --tags) || {
            print_error "Failed to fetch updates."
            exit 1
        }
    else
        rm -rf "$PAXSY_SRC_DIR"
        git clone --depth 1 "$REPO_URL" "$PAXSY_SRC_DIR" || {
            print_error "Failed to clone repository."
            exit 1
        }
        # Fetch tags to allow version checkout
        (cd "$PAXSY_SRC_DIR" && git fetch --tags --depth 1)
    fi

    cd "$PAXSY_SRC_DIR"
    # Checkout the desired ref
    if ! git checkout "$ref" 2>/dev/null; then
        # If checkout fails, try fetching the ref explicitly
        git fetch origin "$ref" && git checkout FETCH_HEAD || {
            print_error "Could not checkout $ref. Please ensure the tag/branch exists."
            exit 1
        }
    fi

    print_success "Source code ready (commit: $(git rev-parse --short HEAD))."
}

# Build the compiler
build_compiler() {
    print_info "Building Paxsy compiler..."

    if [ "$DRY_RUN" -eq 1 ]; then
        echo "  [dry-run] Would run 'make clean && make -j' in $PAXSY_SRC_DIR"
        return
    fi

    cd "$PAXSY_SRC_DIR"
    make clean 2>/dev/null || true
    make -j"$(nproc 2>/dev/null || echo 1)" || {
        print_error "Build failed."
        exit 1
    }

    if [ ! -f ./paxsy ]; then
        print_error "Binary paxsy not found after build."
        exit 1
    fi

    print_success "Build completed."
    # Display version
    local version_info
    if ./paxsy --version 2>/dev/null; then
        version_info="$(./paxsy --version)"
    else
        version_info="$(git describe --tags --always 2>/dev/null || echo "unknown")"
    fi
    print_info "Built version: $version_info"
}

# Install files (binary, libs, license) and record manifest
do_install() {
    if [ "$DRY_RUN" -eq 1 ]; then
        echo "  [dry-run] Would install binary to $INSTALL_PATH/paxsy"
        echo "  [dry-run] Would install libraries to $PAXSY_LIBRARY_DIR"
        echo "  [dry-run] Would install license to /usr/share/doc/paxsy/"
        return
    fi

    # Check write permissions for INSTALL_PATH
    if ! can_write_to "$INSTALL_PATH"; then
        suggest_local_bin
        # Re-check after possible change
        if ! can_write_to "$INSTALL_PATH"; then
            print_error "No write permission to $INSTALL_PATH. Exiting."
            exit 1
        fi
    fi

    # Ensure library directory exists
    mkdir -p "$PAXSY_LIBRARY_DIR"

    # Determine manifest location (inside library dir)
    MANIFEST_FILE="${PAXSY_LIBRARY_DIR}/.manifest"
    create_manifest

    # Install binary
    install -D -m 755 "$PAXSY_SRC_DIR/paxsy" "$INSTALL_PATH/paxsy" || {
        print_error "Failed to install binary."
        exit 1
    }
    add_to_manifest "$INSTALL_PATH/paxsy"

    # Install license
    if [ -f "$PAXSY_SRC_DIR/LICENSE" ]; then
        mkdir -p /usr/share/doc/paxsy
        install -D -m 644 "$PAXSY_SRC_DIR/LICENSE" "/usr/share/doc/paxsy/LICENSE" || {
            print_warning "Failed to install license. Skipping."
        }
        add_to_manifest "/usr/share/doc/paxsy/LICENSE"
    fi

    # Install libraries
    if [ -d "$PAXSY_SRC_DIR/lib" ]; then
        cp -r "$PAXSY_SRC_DIR/lib/"* "$PAXSY_LIBRARY_DIR/" || {
            print_warning "Failed to install libraries. Skipping."
        }
        # Add each library file to manifest (recursively)
        find "$PAXSY_LIBRARY_DIR" -type f -not -name ".manifest" -print0 | while IFS= read -r -d '' file; do
            add_to_manifest "$file"
        done
    fi

    # Also add the manifest itself to the list? Not needed for removal.
    print_success "Installation complete to $INSTALL_PATH/paxsy"

    # Check if PATH includes INSTALL_PATH
    if [[ ":$PATH:" == *":$INSTALL_PATH:"* ]]; then
        print_success "Installation directory is in your PATH. You can run 'paxsy' from anywhere."
    else
        print_warning "Installation directory ($INSTALL_PATH) is not in your PATH."
        echo "To add it temporarily: export PATH=\"$INSTALL_PATH:\$PATH\""
        echo "To add it permanently, add the following line to your ~/.bashrc or ~/.zshrc:"
        echo "  export PATH=\"$INSTALL_PATH:\$PATH\""
    fi

    # Show quick help
    echo ""
    print_info "Paxsy is ready! Try:"
    echo "  paxsy --help"
    echo "  paxsy --version"
    echo "Documentation: https://paxsy.org/docs"
}

# Uninstall Paxsy
do_uninstall() {
    print_info "Uninstalling Paxsy..."

    # Determine manifest location based on current paths or default
    local manifest_path="${PAXSY_LIBRARY_DIR}/.manifest"
    if [ ! -f "$manifest_path" ]; then
        # Try to find it in common locations
        if [ -f "/usr/local/lib/paxsy/.manifest" ]; then
            manifest_path="/usr/local/lib/paxsy/.manifest"
        elif [ -f "${HOME}/.local/lib/paxsy/.manifest" ]; then
            manifest_path="${HOME}/.local/lib/paxsy/.manifest"
        else
            print_error "Manifest file not found. Cannot uninstall automatically."
            echo "Please manually remove files installed by Paxsy."
            exit 1
        fi
    fi

    if [ "$DRY_RUN" -eq 1 ]; then
        echo "  [dry-run] Would read manifest $manifest_path and delete listed files."
        return
    fi

    # Read manifest and delete files if checksum matches
    local removed=0 failed=0
    while IFS= read -r line; do
        if [ -z "$line" ]; then continue; fi
        local file hash
        file="$(echo "$line" | cut -d' ' -f1)"
        hash="$(echo "$line" | cut -d' ' -f2-)"
        if [ -f "$file" ]; then
            if [ -n "$hash" ]; then
                local current_hash
                current_hash="$(compute_sha256 "$file")"
                if [ "$current_hash" != "$hash" ]; then
                    print_warning "Checksum mismatch for $file. Skipping removal (maybe not Paxsy-owned)."
                    failed=$((failed + 1))
                    continue
                fi
            fi
            rm -f "$file" && removed=$((removed + 1)) || {
                print_warning "Failed to remove $file"
                failed=$((failed + 1))
            }
        fi
    done < "$manifest_path"

    # Also remove the manifest and library directory if empty
    if [ -f "$manifest_path" ]; then
        rm -f "$manifest_path"
    fi
    # Remove library directory if empty
    local libdir="$(dirname "$manifest_path")"
    if [ -d "$libdir" ] && [ -z "$(ls -A "$libdir" 2>/dev/null)" ]; then
        rmdir "$libdir" 2>/dev/null || true
    fi

    print_success "Uninstalled $removed files. $failed failures."
    if [ $failed -gt 0 ]; then
        exit 1
    fi
}

# Main execution
main() {
    # Parse command line arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            --help|-h)
                cat <<EOF
Usage: $0 [OPTIONS]

Paxsy bootstrap installer from source (GitHub).

Options:
  --help, -h          Show this help message.
  --branch <branch>   Build a specific branch (overrides BRANCH env var).
  --version <tag>     Build a specific tag/commit (overrides PAXSY_VERSION).
  --dry-run           Show what would be done without making changes.
  --uninstall         Remove all installed Paxsy files (using manifest).

Environment variables:
  INSTALL_PATH        Destination for binary (default: /usr/local/bin)
  PAXSY_LIBRARY_DIR   Destination for libraries (default: /usr/local/lib/paxsy)
  PAXSY_SRC_DIR       Build directory (default: /tmp/paxsy-build)
  BRANCH              Git branch to build (default: main)
  PAXSY_VERSION       Git tag/commit to build (overrides BRANCH)
  NO_COLOR            Disable colored output if set.

This script is downloaded from https://paxsy.org. Please verify its integrity.
EOF
                exit 0
                ;;
            --branch)
                shift
                BRANCH="$1"
                shift
                ;;
            --version)
                shift
                PAXSY_VERSION="$1"
                shift
                ;;
            --dry-run)
                DRY_RUN=1
                shift
                ;;
            --uninstall)
                UNINSTALL=1
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done

    print_info "Paxsy Bootstrap Installer (from paxsy.org)\n"
    echo ""

    if [ "$UNINSTALL" -eq 1 ]; then
        do_uninstall
        exit 0
    fi

    # If PAXSY_VERSION is set, it overrides BRANCH
    if [ -n "$PAXSY_VERSION" ]; then
        print_info "Using version: $PAXSY_VERSION"
    else
        print_info "Using branch: $BRANCH"
    fi

    # Check dependencies (but don't auto-install)
    check_dependencies

    # Fetch source
    fetch_source

    # Build
    build_compiler

    # Install
    do_install

    echo ""
    print_success "All done! 🎉"
}

# Run main (pass all arguments)
main "$@"
