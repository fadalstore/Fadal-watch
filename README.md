# FadalWatch

FadalWatch contains the Fadal Kernel BIOS image, Fadal64 UEFI loader/payload, UTM iOS package, and native iOS source.

## Download from any terminal

The repository is private, so the recommended terminal workflow uses the GitHub CLI. Install `gh`, run `gh auth login` once, and then run:

```sh
gh repo clone fadalstore/Fadal-watch "$HOME/FadalWatch-repo" -- --depth 1
bash "$HOME/FadalWatch-repo/scripts/install-fadalwatch.sh" "$HOME/FadalWatch"
```

The installer downloads the UTM package, native iOS source archive, and BIOS image into `~/FadalWatch`, then verifies SHA-256 checksums. When `gh` is authenticated, downloads use the GitHub Contents API so private-repository permissions are preserved. To choose another directory:

```sh
bash "$HOME/FadalWatch-repo/scripts/install-fadalwatch.sh" "$HOME/Downloads/FadalWatch"
```

### curl or wget without the GitHub CLI

For a private repository, provide a GitHub fine-grained token with read-only **Contents** permission. Keep the token in an environment variable rather than placing it in shell history:

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

If the repository is made public later, the installer can also be fetched directly:

```sh
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch.sh > install-fadalwatch.sh
less install-fadalwatch.sh
bash install-fadalwatch.sh
```

After downloading, extract `FadalWatch-UTM-iOS.zip` and import the `.utm` bundle into UTM. On iOS, select **Emulate**, not **Virtualize**, because the guest is x86_64.

A native `.ipa` still requires Apple signing and provisioning. The source archive is ready to open in Xcode on macOS.
