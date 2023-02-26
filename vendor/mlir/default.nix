{ llvmPackages, lld, cmake, fetchurl, ninja, python3, git }:
let
  stdenv = llvmPackages.stdenv;
in
stdenv.mkDerivation {
  pname = "mlir";
  version = "15.0.7";
  #builder = ./builder.sh;
  src = fetchurl {
    url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/llvm-project-15.0.7.src.tar.xz";
    sha256 = "1ipaxl6jhd6jhnl6skjl7k5jk9xix0643fdhy56z130jnhjcnpwb";
  };
  buildInputs = [ cmake ninja python3 git lld ];
  cmakeFlags = with stdenv; [
    "-DLLVM_ENABLE_PROJECTS=llvm;mlir;lld"
    "-DLLVM_BUILD_EXAMPLES=OFF"
    "-DLLVM_TARGETS_TO_BUILD=X86"
    "-DLLVM_ENABLE_ASSERTIONS=ON"
    "-DBUILD_SHARED_LIBS=ON"
    "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    "-DCMAKE_C_COMPILER=clang"
    "-DCMAKE_CXX_COMPILER=clang++"
    "-DLLVM_ENABLE_LLD=ON"
  ];
  separateDebugInfo = true;
  dontStrip = true;
  cmakeDir = "../llvm";
}
