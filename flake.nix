{
  description = "fpga-assembler";

  inputs = {
    nixpkgs = {
      url = "github:NixOS/nixpkgs/nixos-unstable";
    };

    flake-parts = {
      url = "github:hercules-ci/flake-parts";
      inputs.nixpkgs-lib.follows = "nixpkgs";
    };

    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  nixConfig = {
    extra-substituters = [
      "https://fpga-assembler.cachix.org"
    ];
    extra-trusted-public-keys = [
      "fpga-assembler.cachix.org-1:yp4kNzY1Hru9BlaoE025RuBaL/sX6Ou2abo5L8SHk0I="
    ];
  };

  outputs =
    inputs@{ flake-parts, nixpkgs, ... }:
    flake-parts.lib.mkFlake
      {
        inherit inputs;
      }
      {
        imports = [
          inputs.treefmt-nix.flakeModule
        ];

        systems = [
          "aarch64-darwin"
          "aarch64-linux"
          "x86_64-darwin"
          "x86_64-linux"
        ];

        perSystem =
          { pkgs, system, ... }:
          {
            treefmt = {
              projectRootFile = "flake.nix";
              programs.yamlfmt.enable = true;
              programs.nixfmt.enable = true;
              programs.clang-format.enable = true;
              # Need to figure out how to run hzeller script for faster
              # checks.
              #programs.clang-tidy.enable = true;
            };
            devShells.default =
              with pkgs;
              mkShell {
                packages = [
                  git
                  bazel_8
                  jdk
                  bash
                  gdb

                  # For clang-tidy and clang-format.
                  clang-tools

                  # For buildifier, buildozer.
                  bazel-buildtools
                  bant

                  # Profiling and sanitizers.
                  perf
                  pprof
                  valgrind
                  # Bazel build currently broken.
                  # Uncomment once resolved.
                  #perf_data_converter

                  # FPGA utils.
                  openfpgaloader
                ];

                CLANG_TIDY = "${clang-tools}/bin/clang-tidy";
                CLANG_FORMAT = "${clang-tools}/bin/clang-format";
              };

            # Package fpga-assembler.
            packages.default =
              let
                src = pkgs.nix-gitignore.gitignoreSourcePure [ ] ./.;
                registry = pkgs.fetchFromGitHub {
                  owner = "bazelbuild";
                  repo = "bazel-central-registry";
                  rev = "6873d34b26b6b294a80c7e4bd2cda1926fdfcc4d";
                  hash = "sha256-iMjT8jar5x2JYl9OJoGrjljxtK0elwMsOYa1xnNVe6M=";
                };

                repoCache = pkgs.stdenv.mkDerivation {
                  name = "fpga-as-repo-cache";
                  inherit src;

                  nativeBuildInputs = [
                    pkgs.bazel_8
                    pkgs.git
                    pkgs.cacert
                  ];

                  outputHashMode = "recursive";
                  outputHashAlgo = "sha256";
                  # Trigger a build to get the new hash for the targeted repository cache
                  outputHash =
                    {
                      x86_64-linux = "sha256-+m6lY9af3oZ4X5ZpcB8moql87YxbDJX1x5dYBwQjE8M=";
                    }
                    .${system} or (throw "No hash for system: ${system}");

                  buildPhase = ''
                    export HOME=$(mktemp -d)
                    export USER="nix"
                    export GIT_SSL_CAINFO="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"
                    export SSL_CERT_FILE="${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt"

                    # Analyzing //... ensures we download test deps like googletest
                    bazel build //... \
                      --nobuild \
                      --registry=file://${registry} \
                      --repository_cache=$out \
                      --curses=no \
                      --jobs=$NIX_BUILD_CORES
                  '';
                  installPhase = "true"; # Output is already generated directly into $out
                  dontFixup = true;
                };

              in
              pkgs.stdenv.mkDerivation {
                pname = "fpga-as";
                version = "0.0.1";
                inherit src;

                nativeBuildInputs = with pkgs; [
                  jdk
                  git
                  bash
                  bazel_8
                  xorg.lndir # Required for the cache linking
                ];

                postPatch = ''
                  patchShebangs scripts/create-workspace-status.sh
                '';

                preBuild = ''
                  export HOME=$(mktemp -d)
                  # Tell Bazel where the Nix bash is
                  export BAZEL_SH="${pkgs.bash}/bin/bash"

                  mkdir repo_cache
                  lndir -silent ${repoCache} repo_cache
                '';
                buildPhase = ''
                  runHook preBuild

                  bazel build //fpga:fpga-as \
                    -c opt \
                    --registry=file://${registry} \
                    --repository_cache=$(pwd)/repo_cache \
                    --spawn_strategy=standalone \
                    --curses=no \
                    --jobs=$NIX_BUILD_CORES

                  runHook postBuild
                '';

                doCheck = true;

                checkPhase = ''
                  runHook preCheck

                  # 1. Force Bazel to unpack its internal tools into the cache
                  BAZEL_INSTALL_BASE=$(bazel info install_base \
                    --registry=file://${registry} \
                    --repository_cache=$(pwd)/repo_cache)

                  # 2. Just patch shebang!
                  patchShebangs "$BAZEL_INSTALL_BASE"

                  # 3. Now run the tests
                  bazel test //... \
                    -c opt \
                    --registry=file://${registry} \
                    --repository_cache=$(pwd)/repo_cache \
                    --test_output=errors \
                    --spawn_strategy=standalone \
                    --curses=no \
                    --jobs=$NIX_BUILD_CORES
                  runHook postCheck
                '';
                installPhase = ''
                  runHook preInstall
                  install -D --strip bazel-bin/fpga/fpga-as "$out/bin/fpga-as"
                  runHook postInstall
                '';

                meta = {
                  description = "Tool to convert FASM to FPGA bitstream.";
                  homepage = "https://github.com/lromor/fpga-assembler";
                  license = pkgs.lib.licenses.asl20;
                  platforms = pkgs.lib.platforms.linux;
                };
              };
          };
      };
}
