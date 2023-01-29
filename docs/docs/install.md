
Install
=========================

Nix Flake
-------------------------

isQ is built with [Nix Flakes](https://nixos.wiki/wiki/Flakes), making it super easy to obtain when you have Nix installed:

```bash
# Add isQ binary cache to Cachix to prevent building from source.
nix-shell -p cachix --run "cachix use arclight-quantum"
# Enter the environment with isQ installed.
nix shell github:arclight-quantum/isQ-Compiler
# Now isQ is placed in $PATH.
isqc --version
```

Or you may create a project folder pinned to a compiler version.

```bash
nix flake new --template github:arclight-quantum/isQ-Compiler hello-quantum
cd hello-quantum && nix develop
```


Binary Tarball
-------------------------

We provide a traditional binary tarball based on [nix-user-chroot](https://github.com/nix-community/nix-user-chroot). Note that the tarball requires Linux kernel user namespaces to work.

```bash
# Check if user namespace is supported for your Linux kernel.
unshare --user --pid echo YES
# Download and unpack tarball.
wget https://github.com/arclight-quantum/isQ-Compiler/releases/download/0.0.1/isqc-0.0.1.tar.gz
tar -xvf isqc-0.0.1.tar.gz
# Now isQ is here.
./isqc --version
```


Docker Container
-------------------------
We provide two Docker images with isQ compiler builtin: one for normal users providing a full Ubuntu environment, and the other for professional Docker users with only binary files necessary for isQ.

```bash
# Ubuntu-based Docker image.
docker run -it arclightquantum/isqc:ubuntu-0.0.1 bash
isqc --version # Run in container.
# Binary only Docker image.
docker run --rm -v $(pwd):/workdir arclightquantum/isqc:0.0.1 isqc --version
```


