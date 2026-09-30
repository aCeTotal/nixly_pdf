{
  description = "Nixly PDF - fast PDF reader, editor and signer";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        fontsRev = "5174b3333331c966c38f4355d50b03ca1c1df2f9";
        signatureFonts = pkgs.linkFarm "nixly-pdf-signature-fonts" (map
          (font: {
            name = baseNameOf font.path;
            path = pkgs.fetchurl {
              url = "https://raw.githubusercontent.com/google/fonts/${fontsRev}/${font.path}";
              inherit (font) hash;
            };
          })
          [
            { path = "ofl/mrssaintdelafield/MrsSaintDelafield-Regular.ttf"; hash = "sha256-Z6erwpjOnTaLLAD8v/UuxUvpSIibilkDKugMozIuWzQ="; }
            { path = "ofl/mrdehaviland/MrDeHaviland-Regular.ttf"; hash = "sha256-EM2ONLF6QnItxJ0tYrf+d0WVq+NMyJ7h15Zj32jqYIk="; }
            { path = "ofl/cedarvillecursive/Cedarville-Cursive.ttf"; hash = "sha256-X4bXHQimhgXVkSuby2OaBgzpwN69fgtxX14mjUpLwxc="; }
            { path = "ofl/labelleaurore/LaBelleAurore.ttf"; hash = "sha256-7WdGKZngXwzayS9oY3RmHk1oxW/c19BXJcbfe0HqvSo="; }
            { path = "ofl/dawningofanewday/DawningofaNewDay.ttf"; hash = "sha256-Kyr6Yj27GSbjsCZgMoe00V7gV3gQbiansz75TUvincs="; }
            { path = "ofl/zeyada/Zeyada.ttf"; hash = "sha256-CfI9DXi24WbduEgHk7tVCrTCqvZgLtpHo5T+6T0qlmc="; }
            { path = "ofl/nothingyoucoulddo/NothingYouCouldDo.ttf"; hash = "sha256-Ha+M95B2v1nFqRF7Xv1uzqNeV6Be8Sf+T5WwcrilJF0="; }
            { path = "ofl/kristi/Kristi-Regular.ttf"; hash = "sha256-ZyW3oo2b2HYeKDSmqzgLq+BzZ4xvQgF/5XYRa51v0qA="; }
            { path = "ofl/greatvibes/GreatVibes-Regular.ttf"; hash = "sha256-jVCYAhhvG1FXJTHs8xPoCY+aW/36ypPwybNEZ/mYLRU="; }
            { path = "ofl/alexbrush/AlexBrush-Regular.ttf"; hash = "sha256-33AgONjid5cjDHeVnBOe7qOMrAyvU+GepbUT07DTNi0="; }
            { path = "ofl/allura/Allura-Regular.ttf"; hash = "sha256-nBQrLlFYMsDfxP+LjqGPQDFJQ7+Te3LisjxGYbrBTMY="; }
            { path = "ofl/sacramento/Sacramento-Regular.ttf"; hash = "sha256-k0H9oQrb/rfvyUMCs0UHo+In1+f1xDLfP1rIdT/3PSQ="; }
            { path = "ofl/parisienne/Parisienne-Regular.ttf"; hash = "sha256-vJ7hfwIuILxwB5fl9VfRS/pDrwyY2ebJxcHKTseqzVc="; }
            { path = "ofl/pinyonscript/PinyonScript-Regular.ttf"; hash = "sha256-SqsTCm7Sf4uBF3OMhKVgLt+TAM3MBlGpplv5b0Uawpo="; }
            { path = "ofl/mrdafoe/MrDafoe-Regular.ttf"; hash = "sha256-pCo8ZqcJkn81gkP9/LxDe9jrGI4u5qaAxS9b4SxnD9I="; }
            { path = "ofl/herrvonmuellerhoff/HerrVonMuellerhoff-Regular.ttf"; hash = "sha256-uorBCAenlGK3yCZbLuu4QZ5QF/3BpYKLGgcdnoR4dy4="; }
            { path = "ofl/monsieurladoulaise/MonsieurLaDoulaise-Regular.ttf"; hash = "sha256-3T4E++Zhp6cP+HtWtEbleiYF3lZZyE0171j0YG7mj60="; }
            { path = "apache/homemadeapple/HomemadeApple-Regular.ttf"; hash = "sha256-3RuqyjzeGx+EFa7TsK6mgIZVydnKWpnHKC2azMFsGlg="; }
          ]);

        fallbackFonts = pkgs.symlinkJoin {
          name = "nixly-pdf-fallback-fonts";
          paths = [ pkgs.liberation_ttf pkgs.carlito pkgs.caladea pkgs.dejavu_fonts ];
        };

        nixly-pdf = pkgs.stdenv.mkDerivation {
          pname = "nixly-pdf";
          version = "0.1";
          src = ./.;

          nativeBuildInputs = [
            pkgs.meson
            pkgs.ninja
            pkgs.pkg-config
            pkgs.qt6.wrapQtAppsHook
          ];

          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtwayland
            pkgs.qt6.qtsvg
            pkgs.mupdf
            pkgs.fontconfig
            (pkgs.tesseract.override { enableLanguages = [ "eng" "nor" ]; })
          ];

          qtWrapperArgs = [ "--prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.libreoffice ]}" ];

          mesonFlags = [
            "-Dsignature_fonts=${signatureFonts}"
            "-Dfallback_fonts=${fallbackFonts}/share/fonts"
          ];
        };
      in {
        packages.default = nixly-pdf;

        apps.default = {
          type = "app";
          program = "${nixly-pdf}/bin/nixly-pdf";
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ nixly-pdf ];
          packages = [ pkgs.libreoffice ];
          SIGNATURE_FONTS = signatureFonts;
          FALLBACK_FONTS = "${fallbackFonts}/share/fonts";
        };
      }
    );
}
