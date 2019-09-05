# xwintoggle
This project allows users to toggle X11 windows using globally-bound keyboard
shortcuts. Its intention is to replicate functionality of similar programs in
the OSX ecosystem (e.g., Apptivate).

## Installation
First install the dependencies:
```
sudo apt update && sudo apt install libx11-dev yaml-cpp-dev
```

Then clone the repository:
```
git clone git@github.com:jeffeverett/xwintoggle.git
```

Then build the program:
```
cd xwintoggle
make -j4
```

The program can then be run using:
```
./xwintoggle
```

## Configuration
Configuration settings are stored in YAML. Their default location is
`~/.xwintoggle/config.yaml`. However, this default can be overwritten by
invoking the program like so:

```
./xwintoggle $CONFIG_PATH
```

The following is an example configuration:
```
launchData:
- key: F1
  binPath: /opt/google/chrome/chrome
  args:
  - --disable-gpu-vsync
  - --disable-gpu-sandbox
- key: F2
  binPath: /usr/bin/gnome-terminal
  toggleBinPath: /usr/lib/gnome-terminal/gnome-terminal-server
```

The allowed settings are as follows:
- `launchData` (required): List of dictionaries. Each dictionary contains data
  that controls how windows of specified programs are managed
  by `xwintoggle`.
  - `key` (required): The key (as in keyboard) to be used
    to toggle this program.
  - `binPath` (required): The binary path to be used to create an instance of
    this program if it is not already running.
  - `toggleBinPath` (optional): The binary path to be used to determine windows
    that belong to instances of this program. If not specified, defaults to
    `binPath`.
  - `args` (optional): A list of arguments to send to the program when it is
     first created.

## Notes
- The reasoning for separate `binPath` and `toggleBinPath` arguments is as
  follows: some programs exist only to ease the creation of one or multiple
  separate processes. These processes may belong to different executables,
  in which case we are no longer able to determine the relevant window(s) using
  the `binPath`. In this case, we must instead use the executable path
  corresponding to the spawned process that owns the window(s) we wish to
  toggle; this information is given by `toggleBinPath`.
- To determine the `PID` of a given window, we check the `_NET_WM_PID` of that
  window. This is a window property that is set by the vast majority of 
  raphical programs, but not all graphical programs. If a program does not
  specify this property, then `xwintoggle` is unable to toggle windows
  belonging to that program.