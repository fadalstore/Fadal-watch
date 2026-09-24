# FadalWatch

FadalWatch contains the Fadal Kernel BIOS image, Fadal64 UEFI loader/payload, UTM iOS package, and native iOS source.

## Continue development from a clean checkout

The repository is organized so that another engineer or AI agent can continue
without reconstructing the project history. Read [AI_HANDOFF.md](AI_HANDOFF.md)
first, then consult [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and
[docs/BUILD.md](docs/BUILD.md).

```sh
git clone https://github.com/fadalstore/Fadal-watch.git
cd Fadal-watch
scripts/validate-repo.sh
```

If QEMU is not installed yet, the validator automatically performs the
freestanding compile and independence checks and prints the exact package
command needed for the full smoke test. Use `scripts/validate-repo.sh
--require-qemu` when a missing emulator must be treated as an error.

The canonical bootstrap command is:

```sh
scripts/bootstrap-fadalwatch.sh
```

Use `scripts/bootstrap-fadalwatch.sh --uefi` when GNU-EFI is installed and the
UEFI image should also be regenerated. Use `scripts/validate-repo.sh --uefi`
for the narrower UEFI validation path.

## Download from any terminal

The repository is public, so the simplest terminal workflow requires only `curl` and `bash`:

```sh
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch.sh | bash
```

The installer also works through the GitHub CLI if you prefer a cloned checkout:

```sh
gh repo clone fadalstore/Fadal-watch "$HOME/FadalWatch-repo" -- --depth 1
bash "$HOME/FadalWatch-repo/scripts/install-fadalwatch.sh" "$HOME/FadalWatch"
```

The installer downloads the UTM package, native iOS source archive, and BIOS image into `~/FadalWatch`, then verifies SHA-256 checksums. When `gh` is authenticated, downloads use the GitHub Contents API so private-repository permissions are preserved. To choose another directory:

```sh
bash "$HOME/FadalWatch-repo/scripts/install-fadalwatch.sh" "$HOME/Downloads/FadalWatch"
```

### curl or wget without the GitHub CLI

If an administrator later makes the repository private again, provide a GitHub fine-grained token with read-only **Contents** permission. Keep the token in an environment variable rather than placing it in shell history:

```sh
export FADALWATCH_TOKEN='github_pat_...'
curl -fsSL \
  -H "Authorization: Bearer $FADALWATCH_TOKEN" \
  -H 'Accept: application/vnd.github.raw' \
  'https://api.github.com/repos/fadalstore/Fadal-watch/contents/scripts/install-fadalwatch.sh?ref=main' \
  -o /tmp/install-fadalwatch.sh
bash /tmp/install-fadalwatch.sh "$HOME/FadalWatch"
```

The installer uses `FADALWATCH_TOKEN` with `curl` or `wget` against the GitHub Contents API. `GH_TOKEN` is also accepted. Do not share the token or commit it to a file.

## Install directly in Termux

FadalWatch has a dedicated Termux installer. It stores the runtime under
`$PREFIX/opt/fadalwatch`, installs a `fadalwatch` launcher in `$PREFIX/bin`,
downloads the verified BIOS image, and does not require root access:

```sh
pkg update
pkg install curl coreutils
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch-termux.sh | bash
fadalwatch info
```

Install the optional Termux build and emulation tools with:

```sh
fadalwatch tools
```

The BIOS image can then be started from Termux with:

```sh
fadalwatch run
```

The launcher attaches a QEMU `rtl8139` PCI device to QEMU user networking by
default. During boot, FadalOS should report the RTL8139 discovery and Ethernet,
ARP, and IPv4 foundation checks. The Termux host does not expose its physical
Wi-Fi adapter directly to the guest; QEMU provides a user-mode NAT boundary.
Set `FADALWATCH_NETWORK=0` to boot without the virtual NIC.

```sh
fadalwatch run
FADALWATCH_NETWORK=0 fadalwatch run
```

The current FadalOS network layer validates NIC discovery, RX/TX DMA setup,
Ethernet-II, ARP, IPv4 checksums, UDP/TCP prototypes, and loopback boundaries.
It is not yet a complete HTTPS client, DHCP client, or native Git smart-HTTP
implementation. Therefore successful RTL8139 detection does not by itself
promise Internet access from the FadalOS shell.

Run the focused network diagnostic with:

```sh
fadalwatch netcheck
```

The command boots a short-lived QEMU instance, captures the serial trace, and
fails unless RTL8139 initialization, the ARP parser/cache self-test, and the
IPv4 next-hop routing self-test are all reported.

The launcher also provides `fadalwatch build` for cloning and building the
kernel source, `fadalwatch update` for refreshing the installation, and
`fadalwatch uninstall` for removing it. The Android device must have enough
storage for QEMU and the source tree; FadalOS itself remains an x86 BIOS guest
and is emulated by QEMU rather than executed as an Android kernel.

If the repository is made public later, the installer can also be fetched directly:

```sh
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch.sh > install-fadalwatch.sh
less install-fadalwatch.sh
bash install-fadalwatch.sh
```

After downloading, extract `FadalWatch-UTM-iOS.zip` and import the `FadalWatch-UEFI.utm` bundle into UTM or UTM SE. On iOS, select **Emulate**, not **Virtualize**, because the guest is x86_64. The current package uses UTM configuration version 4 and includes a raw UEFI disk, RTL8139 NIC, and serial Terminal configuration.

A native `.ipa` still requires Apple signing and provisioning. The source archive is ready to open in Xcode on macOS.
