#!/usr/bin/env bash
set -e

GREEN='\033[0;32m'; CYAN='\033[0;36m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'

log()  { echo -e "${GREEN}[✓]${NC} $1"; }
info() { echo -e "${CYAN}[i]${NC} $1"; }
warn() { echo -e "${YELLOW}[!]${NC} $1"; }
err()  { echo -e "${RED}[✗]${NC} $1"; exit 1; }

echo -e "${CYAN}"
echo "   ████████╗███████╗ █████╗ ██╗████████╗ ██████╗ ███████╗"
echo "   ╚══██╔══╝██╔══██╗██╔══██╗██║╚══██╔══╝██╔═══██╗██╔════╝"
echo "      ██║   ██████╔╝███████║██║   ██║   ██║   ██║███████╗"
echo "      ██║   ██╔══██╗██╔══██║██║   ██║   ██║   ██║╚════██║"
echo "      ██║   ██║  ██║██║  ██║██║   ██║   ╚██████╔╝███████║"
echo "      ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝   ╚═╝    ╚═════╝ ╚══════╝"
echo -e "${NC}"
echo -e "  ${YELLOW}RAM-resident x86_64 OS - boots from USB, lives in RAM${NC}"
echo ""

OS="$(uname -s)"

install_deps() {
    if [ "$OS" = "Darwin" ]; then
        info "macOS detected. Installing dependencies via Homebrew..."
        brew install nasm lld xorriso mtools x86_64-elf-grub
    elif command -v apt-get &>/dev/null; then
        info "Debian/Ubuntu detected. Installing dependencies..."
        sudo apt-get install -y nasm xorriso mtools lld \
            grub-pc-bin grub-common grub-efi-amd64-bin grub-efi-ia32-bin
    elif command -v dnf &>/dev/null; then
        info "Fedora/RHEL detected. Installing dependencies..."
        sudo dnf install -y nasm lld xorriso mtools grub2-tools-extra
    elif command -v pacman &>/dev/null; then
        info "Arch detected. Installing dependencies..."
        sudo pacman -Sy --noconfirm nasm lld xorriso mtools grub
    else
        warn "Unknown distro. Make sure you have: nasm clang lld grub-mkrescue xorriso mtools"
    fi
    log "Dependencies ready."
}

grub_mkrescue() {
    command -v x86_64-elf-grub-mkrescue 2>/dev/null || \
    command -v grub-mkrescue 2>/dev/null || \
    command -v grub2-mkrescue 2>/dev/null || \
    err "grub-mkrescue not found. Run: ./build.sh deps"
}

build() {
    log "Building kernel..."
    make all
}

make_iso() {
    log "Building bootable ISO..."
    make iso
}

run_smoke() {
    log "Running host-side filesystem smoke test (no emulator needed)..."
    make smoke
}

run_qemu() {
    if ! command -v qemu-system-x86_64 &>/dev/null; then
        err "qemu-system-x86_64 not found. Install QEMU (macOS: brew install qemu)."
    fi
    info "Launching TrigerOS in QEMU..."
    qemu-system-x86_64 -cdrom trigeros.iso -serial stdio -m 256M -no-reboot
}

write_usb() {
    local dev="$1"
    if [ -z "$dev" ]; then
        if [ "$OS" = "Darwin" ]; then
            diskutil list
        else
            lsblk -d -o NAME,SIZE,TYPE,TRAN
        fi
        echo ""
        read -rp "Enter USB device (e.g. /dev/disk2 or /dev/sdb): " dev
    fi
    [ -e "$dev" ] || err "Device $dev not found."
    echo -e "${RED}WARNING: This will ERASE $dev. Type yes to continue:${NC}"
    read -r confirm
    [ "$confirm" = "yes" ] || { warn "Aborted."; return; }
    make iso
    log "Writing trigeros.iso to $dev ..."
    if [ "$OS" = "Darwin" ]; then
        local base
        base="$(basename "$dev")"
        [[ "$base" == rdisk* ]] || base="r$base"
        diskutil unmountDisk "$dev" 2>/dev/null || true
        sudo dd if=trigeros.iso of="/dev/$base" bs=1m
    else
        sudo dd if=trigeros.iso of="$dev" bs=4M status=progress oflag=sync
    fi
    log "Done! You can now boot from $dev"
}

clean() {
    make clean
    log "Cleaned."
}

case "${1:-all}" in
    deps)    install_deps ;;
    build)   build ;;
    iso)     build && make_iso ;;
    run)     build && make_iso && run_qemu ;;
    smoke)   build && run_smoke ;;
    usb)     write_usb "$2" ;;
    clean)   clean ;;
    all|"")
        build && make_iso
        echo ""
        echo -e "${GREEN}╔═══════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║      TrigerOS built successfully!      ║${NC}"
        echo -e "${GREEN}╚═══════════════════════════════════════╝${NC}"
        echo ""
        echo -e "  ${YELLOW}Verify (no emulator):${NC} ./build.sh smoke"
        echo -e "  ${YELLOW}Run in QEMU:${NC}  ./build.sh run"
        echo -e "  ${YELLOW}Write to USB:${NC} ./build.sh usb /dev/diskX"
        echo -e "  ${YELLOW}ISO file:${NC}     $(pwd)/trigeros.iso"
        echo ""
        ;;
    *)
        echo "Usage: $0 {deps|build|iso|run|smoke|usb [device]|clean|all}"
        exit 1
        ;;
esac
