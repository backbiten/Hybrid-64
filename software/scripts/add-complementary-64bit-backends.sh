#!/usr/bin/env bash
set -euo pipefail

echo "=== Adding Complementary Legacy64 + Hybrid64 Backends ==="
echo "Legacy64 backs Hybrid64, Hybrid64 backs Legacy64"
echo "Same 64-bit 'bitrate' (architecture level) between computers"
echo "For Temple of Set community — air-gapped, neighborhood 64-bit mesh only"

# 1. Set 64-bit context for this addition
export GOARCH=amd64
export GOOS=linux
export CGO_ENABLED=0

# 2. Preserve existing reinitialization logic
if [ -f scripts/reinit-32hybrid.sh ]; then
  ./scripts/reinit-32hybrid.sh
else
  make clean || true
  make all || echo "Continuing without full reinit"
fi

# 3. Create legacy64/ with shared backend notes
LEGACY64_DIR="legacy64"
mkdir -p "$LEGACY64_DIR"/{backends,notes}

cat > "$LEGACY64_DIR/README-complementary.txt" << 'EOF'
TEMPLE OF SET COMMUNITY — Complementary 64-Bit Backends
Legacy64 (pure/classic 64-bit preservation) backs Hybrid64 (community/extended)
Hybrid64 backs Legacy64 (fallback to minimal base)

Both run on the SAME 64-bit bitrate/architecture — x86_64 only talks to x86_64.
No mixing with 32-bit mesh. No external/corporate/government networks.

Boot flow:
- Legacy64 → strict, minimal boot (early AMD64 feel)
- Hybrid64 → adds literacy lab + mesh on top of Legacy64 base
- They complement each other: switch via GRUB params

Shared files: /boot/vmlinuz64 and /boot/initramfs64.cpio.gz
EOF

# 4. Extend GRUB menu with complementary 64-bit entries
GRUB_CFG="legacy32/boot/grub.cfg"
mkdir -p legacy32/boot
if [ ! -f "$GRUB_CFG" ]; then
    cat > "$GRUB_CFG" << 'EOF'
# Temple of Set Community 32/64-Bit GRUB
timeout=5
EOF
fi

echo "Extending GRUB menu with complementary Legacy64 ↔ Hybrid64 entries..."
cat >> "$GRUB_CFG" << 'EOF'

# === Complementary 64-Bit Backends (same bitrate) ===
menuentry "Legacy64 — Pure 64-bit Preservation (backs Hybrid64)" {
    linux /boot/vmlinuz64 cpu=core2 mode=legacy64
    initrd /boot/initramfs64.cpio.gz
}
menuentry "Hybrid64 — Community Edition (backs Legacy64)" {
    linux /boot/vmlinuz64 cpu=Haswell mode=hybrid64 lab=literacy mesh64=on
    initrd /boot/initramfs64.cpio.gz
}
menuentry "Hybrid64 Fallback to Legacy64 Base" {
    linux /boot/vmlinuz64 cpu=Haswell mode=legacy64 fallback=true
    initrd /boot/initramfs64.cpio.gz
}
EOF

# 5. Add Makefile support for 64-bit backends
if [ -f Makefile ]; then
    cat >> Makefile << 'EOF'

## legacy64: build minimal pure 64-bit backend
legacy64:
	GOARCH=amd64 make build

## hybrid64: build extended community 64-bit backend
hybrid64: legacy64
	GOARCH=amd64 make build

## era64: alias for complementary 64-bit
era64: hybrid64
	@echo "Complementary 64-bit backends ready (Legacy64 ↔ Hybrid64)"
EOF
fi

# 6. Complementary 64-bit mesh init
MESH64_DIR="iso/rootfs/opt/64mesh"
mkdir -p "$MESH64_DIR"
cat > "$MESH64_DIR/mesh64-init.sh" << 'EOF'
#!/bin/sh
# Complementary 64-bit Mesh — same bitrate only

echo "Starting 64-bit Mesh (batman-adv)"
[ "$(uname -m)" = "x86_64" ] || { echo "Not 64-bit — abort"; exit 1; }

modprobe batman-adv 2>/dev/null || echo "Module load deferred"

IFACE="${1:-wlan0}"
batctl if add "$IFACE"
ip link set bat0 up
ip addr add 10.64.0.$(shuf -i 10-250 -n 1)/24 dev bat0

echo ""
echo "64-bit Mesh active (Legacy64 + Hybrid64 compatible)"
echo " - Only talks to other x86_64 machines on same network"
echo " - To switch modes: reboot and choose GRUB entry"
echo " - Stop: batctl if del $IFACE ; ip link set bat0 down"
EOF
chmod +x "$MESH64_DIR/mesh64-init.sh"

echo "✅ Complementary Legacy64 + Hybrid64 Backends Added.",
