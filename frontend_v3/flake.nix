{
  description = "isQ Frontend";
  inputs = {
    isqc-base.url = "path:../base";
    gitignore.url = "github:hercules-ci/gitignore.nix";
    rust-overlay.url = "github:oxalica/rust-overlay";
    vendor.url = "path:../vendor";
  };
  outputs = { self, isqc-base, gitignore, rust-overlay, vendor, ... }:
    isqc-base.lib.isqc-components-flake {
      inherit self;
      overlay = isqc-base.lib.isqc-override (pkgs: final: prev: {
        #isq-simulator-plugin-cuda = (final.callPackage ./plugins/cuda-plugin { });
        isq-frontend = (final.callPackage ./default.nix {
          gitignoreSource = gitignore.lib.gitignoreSource;
        });
      });
      components = [ "isq-frontend" ];
      defaultComponent = "isq-frontend";
      preOverlays = [ rust-overlay.overlays.default ];
      depComponentOverlays = [ vendor.overlays.default ];
      #shell = {pkgs}: pkgs.mkShell.override {stdenv = pkgs.llvmPackages.stdenv;} {
      #  inputsFrom = [pkgs.isqc.isq-opt];
      #  nativeBuildInputs = [pkgs.clang-tools];
      #};
    };
}
