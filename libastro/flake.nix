{
  description = ''
    libastro: a modern C++23 library for the JPL DE440/DE441 ephemerides.
    Toolchain mirrors ~/code/josuttis/cppstd23: LLVM/Clang 22 using the
    GCC 15 libstdc++ (with working `import std;`), plus CMake + Ninja + curl.
  '';

  inputs = {
    nixpkgs.url = "https://flakehub.com/f/DeterminateSystems/nixpkgs-weekly/0.1";
  };

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;

      mkFor =
        system:
        let
          pkgs = import nixpkgs { inherit system; };

          llvm = pkgs.llvmPackages_22;
          gcc = pkgs.gcc15;
          gccLibs = gcc.cc;
          gxxVer = gccLibs.version;
          gxxRoot = "${gccLibs}/include/c++/${gxxVer}";
          backwardInc = "${gxxRoot}/backward";

          # Clang 22 wired to consume GCC 15's libstdc++ (see josuttis flake for
          # the rationale on the extra backward/ include).
          clang = llvm.libstdcxxClang.override {
            gccForLibs = gccLibs;
            nixSupport.cc-cflags = [
              "-idirafter"
              backwardInc
            ];
          };

          # libstdc++'s `std` module and its consumers must be built with
          # hardening off and in lock-step; clear the salted hardening vars.
          disableHardening = ''
            unset ''${!NIX_HARDENING_ENABLE*} 2>/dev/null || true
          '';

          devShell = pkgs.mkShellNoCC {
            packages = [
              clang
              llvm.clang-tools
              llvm.lldb
              pkgs.gdb
              pkgs.cmake
              pkgs.ninja
              pkgs.gnumake
              pkgs.curl
              pkgs.cacert
              pkgs.git
              pkgs.pkg-config
            ];

            CC = "${clang}/bin/clang";
            CXX = "${clang}/bin/clang++";

            shellHook = ''
              if [ -e /var/lib/sss/pipes/nss ]; then
                export LD_LIBRARY_PATH="${pkgs.sssd}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
              fi
              ${disableHardening}
              echo ""
              echo "  libastro · C++23 toolchain ready"
              echo "    clang++ : $(clang++ --version | head -n1)"
              echo "    std lib : GCC ${gxxVer} libstdc++"
              echo ""
              echo "    First time:   ./scripts/vendor_mdspan.sh && ./scripts/fetch_ephemeris.sh"
              echo "    Configure:    cmake -B build -G Ninja"
              echo "    Build+test:   cmake --build build && ctest --test-dir build"
              echo ""
            '';
          };
        in
        {
          inherit clang devShell;
        };
    in
    {
      devShells = forAllSystems (system: { default = (mkFor system).devShell; });
      packages = forAllSystems (system: { default = (mkFor system).clang; });
      formatter = forAllSystems (system: (import nixpkgs { inherit system; }).nixfmt-rfc-style);
    };
}
