#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD    1
#define _NET_WM_STATE_TOGGLE 2

#define DEBUG 1

// Global variables
Display *display;
Window rootWindow;


std::vector<Window> findWindowsOwnedByExe(std::string exeName)
{
  std::vector<Window> windows;

  // Loop through all children of root window and check whether the executable names
  // associated with the given PID match the passed in executable name
  Atom pidAtom = XInternAtom(display, "_NET_WM_PID", true);
  Window parentWindow;
  Window *childrenWindows;
  unsigned int numChildren;
  XQueryTree(display, rootWindow, &rootWindow, &parentWindow, &childrenWindows, &numChildren);
  #ifdef DEBUG
    std::cout << numChildren << " children of root window" << std::endl;
  #endif
  for (int i = 0; i < numChildren; i++) {
    Atom type;
    int format;
    unsigned long numItems;
    unsigned long bytesAfter;
    unsigned char *pidProp;
    if (XGetWindowProperty(display, childrenWindows[i], pidAtom, 0, 1, false, AnyPropertyType,
      &type, &format, &numItems, &bytesAfter, &pidProp) == Success && pidProp != 0) {
      unsigned long pid = *((unsigned long *)pidProp);
      std::string filename = "/proc/" + std::to_string(pid) + "/exe";
      char *exeFile = realpath(filename.c_str(), nullptr);
      if (exeFile) {
        if (strcmp(exeFile, exeName.c_str()) == 0) {
          #ifdef DEBUG
            std::cout << "found window with PID " << pid;
            std::ios_base::fmtflags f(std::cout.flags());
            std::cout << " and ID 0x" << std::hex << childrenWindows[i] << "." << std::endl;
            std::cout.flags(f);
          #endif
          windows.push_back(childrenWindows[i]);
        }

        free(exeFile);
      }

      XFree(pidProp);
    }
  }
  if (childrenWindows) {
    XFree(childrenWindows);
  }

  return windows;
}

void handleKeypress(std::string exeName)
{
  Atom stateAtom = XInternAtom(display, "WM_STATE", False);

  // Determine if there is a window owned by the given executable
  auto windows = findWindowsOwnedByExe(exeName);

  for (auto window : windows) {
    Atom type;
    int format;
    unsigned long numItems;
    unsigned long bytesAfter;
    unsigned char *stateProp;
    if (XGetWindowProperty(display, window, stateAtom, 0, 1, false, AnyPropertyType,
      &type, &format, &numItems, &bytesAfter, &stateProp) == Success && stateProp != 0) {
      bool hidden = *((unsigned int *)stateProp) != NormalState;
      if (hidden) {
        XWithdrawWindow(display, window, 0);
        XMapWindow(display, window);
      }
      else {
        XIconifyWindow(display, window, 0);
      }
    }
  }
}

int main(int argc, char *argv [])
{
  // Open connection to the server
  display = XOpenDisplay("");
  rootWindow = DefaultRootWindow(display);

  // Grab relevant keyboard shortcuts
  int f1Keycode = XKeysymToKeycode(display, XK_F1);
  int f2Keycode = XKeysymToKeycode(display, XK_F2);
  XGrabKey(display, f1Keycode, AnyModifier, rootWindow, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(display, f2Keycode, 0, rootWindow, false, GrabModeAsync, GrabModeAsync);

  // Enter the event loop
  XEvent event;
  bool done = false;
  while(!done)
  {
    XNextEvent(display, &event);
    switch(event.type) {
      case KeyPress:
        if (event.xkey.keycode == f1Keycode) {
          handleKeypress("/usr/bin/terminator");
        } else if (event.xkey.keycode == f2Keycode) {
          handleKeypress("/opt/google/chrome/chrome");
        }

        break;
    }
  }

	// Clean up before exiting
  XCloseDisplay(display);
}