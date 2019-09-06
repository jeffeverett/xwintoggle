#include "utils/x11/x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

void printWindowDebug(unsigned long pid, Window window) {
  #ifdef DEBUG
    std::cout << "found window with PID " << pid;
    std::ios_base::fmtflags f(std::cout.flags());
    std::cout << " and ID 0x" << std::hex <<
      window << "." << std::endl;
    std::cout.flags(f);
  #endif
}

namespace Utils {
  bool windowIsViewable(Display *display, Window window) {
    XWindowAttributes windowAttributes;
    XGetWindowAttributes(display, window, &windowAttributes);
    return windowAttributes.map_state == IsViewable;
  }

  void findWindowsOwnedByExeRecursive(Display *display, std::string exeName,
    bool useCmdLine, Window startWindow, std::vector<Window> &windows) {
    // Loop through all children of root window and
    // check whether the executable names associated
    // with the given PID match the passed in executable name
    Atom pidAtom = XInternAtom(display, "_NET_WM_PID", true);
    Window rWindow;
    Window parentWindow;
    Window *childrenWindows;
    unsigned int numChildren;
    XQueryTree(display, startWindow, &rWindow, &parentWindow,
      &childrenWindows, &numChildren);
    for (int i = 0; i < numChildren; i++) {
      // Determine if this window matches desired criteria
      Atom type;
      int format;
      unsigned long numItems;
      unsigned long bytesAfter;
      unsigned char *pidProp;
      if (XGetWindowProperty(display, childrenWindows[i], pidAtom, 0, 1, false,
        AnyPropertyType, &type, &format, &numItems, &bytesAfter, &pidProp)
        == Success && pidProp != 0) {
        unsigned long pid = *((unsigned long *)pidProp);
        if (useCmdLine) {
          std::string filename = "/proc/" + std::to_string(pid) + "/cmdline";
          std::ifstream cmdlineFile(filename);
          if (cmdlineFile.is_open()) {
            std::stringstream ss;
            ss << cmdlineFile.rdbuf();
            std::string cmdline = ss.str();
            if (cmdline.find(exeName) != -1) {
              printWindowDebug(pid, childrenWindows[i]);
              windows.push_back(childrenWindows[i]);
            }
          }
        } else {
          std::string filename = "/proc/" + std::to_string(pid) + "/exe";
          char *exeFile = realpath(filename.c_str(), nullptr);
          if (exeFile) {
            if (strcmp(exeFile, exeName.c_str()) == 0) {
              printWindowDebug(pid, childrenWindows[i]);
              windows.push_back(childrenWindows[i]);
            }

            free(exeFile);
          }
        }
        XFree(pidProp);
      }

      // Recursively check children
      findWindowsOwnedByExeRecursive(display, exeName, useCmdLine,
        childrenWindows[i], windows);
    }
    if (childrenWindows) {
      XFree(childrenWindows);
    }
  }

  std::vector<Window> findWindowsOwnedByExe(Display *display, Window rootWindow,
    std::string exeName, bool useCmdLine) {
    std::vector<Window> windows;
    findWindowsOwnedByExeRecursive(display, exeName, useCmdLine,
      rootWindow, windows);
    return windows;
  }
}  // namespace Utils
