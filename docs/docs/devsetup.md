Development Setup
=========================

To build up development environment for isQ itself, you need to install Nix with Flakes support on a GNU/Linux distro first.


Install Nix Package Manager
-------------------------

We recommend using [Nix installer by Determinate Systems](https://zero-to-nix.com/start/install) to install Nix with Flakes support out of the box.

```bash
# Install Nix.
curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix | sh -s -- install
# Setup Cachix binary cache.
nix-shell -p cachix --run "cachix use arclight-quantum"
```


Enter Development Environment
-------------------------

We provide necessary tools to build isQ components locally.

```bash
git clone git@github.com:arclight-quantum/isQ-Compiler.git && cd isQ-Compiler
# Virtual environment with necessary building tools. The bash prompt should become (nix-isqc:dev).
nix develop
# Virtual environment with a VSCodium. The bash prompt should become (nix-isqc:codium).
nix develop .#codium
```



Git Commit
-------------------------

We use [pre-commit git hooks](https://github.com/cachix/pre-commit-hooks.nix) to:
- prevent accidentally forgetting to update flake.lock;
- format all Nix build scripts.

If a pre-commit git hook prevents you from committing, just re-add the files changed by git hooks and commit again.
