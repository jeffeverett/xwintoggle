#include "utils/config/config.h"
#include "utils/x11/x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD    1
#define _NET_WM_STATE_TOGGLE 2

#define DEBUG 1

// Global variables
Display *display;
Window rootWindow;
Window predicateWindow;
std::unordered_map<int, std::string> grabKeyReqs;

void handleKeypress(ApplicationLaunchData launchData) {
  Atom stateAtom = XInternAtom(display, "WM_STATE", False);

  // Determine if there is a window owned by the given executable
  auto windows = Utils::findWindowsOwnedByExe(
    display, rootWindow, launchData.toggleBinPath, false);

  if (windows.size() > 0) {
    // Toggle open windows
    for (auto window : windows) {
      Atom type;
      int format;
      unsigned long numItems;
      unsigned long bytesAfter;
      unsigned char *stateProp;
      if (XGetWindowProperty(display, window, stateAtom, 0, 1, false,
        AnyPropertyType, &type, &format, &numItems, &bytesAfter,
        &stateProp) == Success && stateProp != 0) {
        auto windowState = *((unsigned int *)stateProp);
        if (windowState == WithdrawnState) {
          continue;
        }
        bool hidden = *((unsigned int *)stateProp) == IconicState;
        bool covered = !Utils::windowIsVisible(display, window, 0.5f);
        if (hidden || covered) {
          std::cout << "Info: Displaying window " << window <<
            " belonging to binary \"" << launchData.binPath << "\"." <<
            std::endl;
          XWithdrawWindow(display, window, 0);
          XMapWindow(display, window);

          struct timespec sleeptime;
          sleeptime.tv_sec = 0;
          sleeptime.tv_nsec = 10000000L;  // 0.01s
          while (!Utils::windowIsViewable(display, window)) {
            nanosleep(&sleeptime, nullptr);
          }
          // Wait for map
          XSetInputFocus(display, window, RevertToNone, CurrentTime);
          XRaiseWindow(display, window);
        } else {
          std::cout << "Info: Hiding window " << window <<
            " belonging to binary \"" << launchData.binPath << "\"." <<
            std::endl;
          XIconifyWindow(display, window, 0);
        }
      }
    }
  } else {
    // Run program in separate process, discard the output
    if (fork() == 0) {
      std::cout << "Info: Launching \"" <<
        launchData.binPath << "\"." << std::endl;
      auto fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
      dup2(fd, STDOUT_FILENO);
      std::vector<char *> argv = {};
      argv.reserve(launchData.args.size() + 2);
      argv.push_back(strdup(launchData.binPath.c_str()));
      for (auto arg : launchData.args) {
        argv.push_back(strdup(arg.c_str()));
      }
      argv.push_back(NULL);
      if (execv(launchData.binPath.c_str(), argv.data()) == -1) {
        std::cout << "Error: Failed to launch program \"" << launchData.binPath
          << std::endl;
      }
    }
  }
}

int x11ErrorHandler(Display *display, XErrorEvent *error) {
  switch (error->request_code) {
    case X_GrabKey:
      std::cout << "Error: Attempt to grab key \"" << grabKeyReqs[error->serial]
        << "\" failed. Ungrab this key or choose another." << std::endl;
      break;
    default:
      std::cout << "Error: Unhandled X11 error for request with opcode "
        << +error->request_code << ". Error code: " << +error->error_code
        << "." << std::endl;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  // Determine location of config file through CLI arguments
  std::string homeDir;
  if ((homeDir = getenv("HOME")).empty()) {
    homeDir = getpwuid(getuid())->pw_dir;
  }
  std::string configFilePath = homeDir + "/.xwintoggle/config.yaml";
  if (argc > 1) {
    configFilePath = argv[1];
  }

  // Specify error handling function
  XSetErrorHandler(x11ErrorHandler);

  // Open connection to the server
  display = XOpenDisplay("");
  rootWindow = DefaultRootWindow(display);
  XSelectInput(display, rootWindow, VisibilityChangeMask);

  // Determine configuration
  Config config = Utils::parseConfig(display, configFilePath);

  // Grab relevant keyboard shortcuts
  for (auto launchData : config.launchData) {
    grabKeyReqs[NextRequest(display)] = launchData.key;
    XGrabKey(display, launchData.keyCode, AnyModifier, rootWindow, false,
      GrabModeAsync, GrabModeAsync);
  }

  // Print start message
  std::cout << "Now listening for specified keys." << std::endl;

  // Enter the event loop
  XEvent event;
  bool done = false;
  while (!done) {
    XNextEvent(display, &event);
    switch (event.type) {
      case KeyPress:
        handleKeypress(config.keyDataMapping[event.xkey.keycode]);
        break;
    }
  }

  // Clean up before exiting
  XCloseDisplay(display);
}
