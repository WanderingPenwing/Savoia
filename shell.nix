{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gnumake
    gcc
    glib
    pkg-config
    xorg.libX11
    xorg.libXft
    xorg.libXrandr
    xorg.libXinerama
    xorg.libXext
    libxcrypt
    fontconfig
    freetype
    imlib2
    webkitgtk
    glib-networking
    gst_all_1.gstreamer
    gst_all_1.gst-plugins-base
    gst_all_1.gst-plugins-good
    gst_all_1.gst-plugins-bad
  ];

  shellHook = ''
    export GIO_EXTRA_MODULES=${pkgs.glib-networking}/lib/gio/modules
  '';
}
