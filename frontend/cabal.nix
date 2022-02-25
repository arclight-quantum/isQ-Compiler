{ mkDerivation, alex, array, base, containers, extra, happy, hpack
, lens, lib, math-functions, mtl, parsec, pretty-simple, text
}:
mkDerivation {
  pname = "isqc-lib";
  version = "0.1.0.0";
  src = ./.;
  isLibrary = true;
  isExecutable = true;
  libraryHaskellDepends = [
    array base containers extra lens math-functions mtl parsec
    pretty-simple text
  ];
  libraryToolDepends = [ alex happy hpack ];
  executableHaskellDepends = [
    array base containers extra lens math-functions mtl parsec
    pretty-simple text
  ];
  executableToolDepends = [ alex happy ];
  prePatch = "hpack";
  homepage = "https://github.com/gjz010/isqv2-frontend#readme";
  license = lib.licenses.bsd3;
}
