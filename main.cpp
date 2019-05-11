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

// Global variables
Display *display;
Window rootWindow;


bool findWindowOwnedByExe(std::string exeName, Window *window)
{
  // Loop through all children of root window and check whether the executable names
  // associated with the given PID match the passed in executable name
  Atom pidAtom = XInternAtom(display, "_NET_WM_PID", true);
  Window parentWindow;
  Window *childrenWindows;
  unsigned int numChildren;
  XQueryTree(display, rootWindow, &rootWindow, &parentWindow, &childrenWindows, &numChildren);
  std::cout << numChildren << " children" << std::endl;
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
          std::cout << "found window with PID " << pid << " and ID " << childrenWindows[i] << "." << std::endl;
          *window = childrenWindows[i];
          return true;
        }

        free(exeFile);
      }

      XFree(pidProp);
    }
  }
  if (childrenWindows) {
    XFree(childrenWindows);
  }

  return false;
}

void handleKeypress(std::string exeName)
{
  // Determine if there is a window owned by the given executable
  Window window;
  bool found = findWindowOwnedByExe(exeName, &window);

  if (found) {
    std::cout << "found" << std::endl;


    Atom stateAtom = XInternAtom(display, "_NET_WM_STATE", False);
    Atom hiddenAtom = XInternAtom(display, "_NET_WM_HIDDEN", False);
    Atom maxHorzAtom = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom maxVertAtom = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    // Toggle visibility of window
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = stateAtom;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = _NET_WM_STATE_TOGGLE;
    xev.xclient.data.l[1] = hiddenAtom;
    XSendEvent(display, rootWindow, false, SubstructureNotifyMask, &xev);
  }
  else {
    std::cout << "not found" << std::endl;
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
          std::cout << "F1 pressed" << std::endl;
          handleKeypress("/usr/bin/tilda");
        } else if (event.xkey.keycode == f2Keycode) {
          std::cout << "F2 pressed" << std::endl;
          handleKeypress("/opt/google/chrome/chrome");
        }

        break;
    }
  }

	// Clean up before exiting
  XCloseDisplay(display);
}