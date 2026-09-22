# FadalWatch

FadalWatch contains the Fadal Kernel BIOS image, Fadal64 UEFI loader/payload, UTM iOS package, and native iOS source.

## Download from any terminal

On macOS, Linux, or a compatible Unix terminal, run:

```sh
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch.sh | bash
```

The installer downloads the UTM package, native iOS source archive, and BIOS image into `~/FadalWatch`, then verifies SHA-256 checksums. To choose another directory:

```sh
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch.sh | bash -s -- "$HOME/Downloads/FadalWatch"
```

The same installer supports systems that have `wget` instead of `curl`. It can also be downloaded and inspected before execution:

```sh
wget -qO- https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch.sh > install-fadalwatch.sh
less install-fadalwatch.sh
bash install-fadalwatch.sh
```

After downloading, extract `FadalWatch-UTM-iOS.zip` and import the `.utm` bundle into UTM. On iOS, select **Emulate**, not **Virtualize**, because the guest is x86_64.

A native `.ipa` still requires Apple signing and provisioning. The source archive is ready to open in Xcode on macOS.
