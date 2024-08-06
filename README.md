# savoia (surf) - simple webkit-based browser
savoia is a simple Web browser based on WebKit/GTK+.

# Patches/Changes from vanilla surf
- Modal patch
- Quit-hotkey
- Search engines patch
	- search with <space> <string>
	- See config.def.h for current available engines
- Fixed manpage accordingly
- Cache/Cookies goes to /tmp
- Patched out the need for gcr (thanks to kyx0r)
- download manager
- bookmarks
- tabs
- suspend

# Requirements
In order to build surf you need GTK+ and Webkit/GTK+ header files.

# Installation
Edit config.mk to match your local setup (surf is installed into
the /usr/local namespace by default).  
Edit config.h or config.def.h to match your tastes, and be careful to change marukuru to dmenu or other app you have installed  
  
Afterwards enter the following command to build and install surf (if
necessary as root):

    make clean install

