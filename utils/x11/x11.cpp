#include "utils/x11/x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include <algorithm>
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

std::vector<Window> getWindows(Display *display, Window rootWindow) {
  Atom pidAtom = XInternAtom(display, "_NET_WM_PID", true);
  Window rWindow;
  Window parentWindow;
  Window *childrenWindows;
  unsigned int numChildren;
  XQueryTree(display, rootWindow, &rWindow, &parentWindow,
    &childrenWindows, &numChildren);

  std::vector<Window> windows = {};
  for (int i = 0; i < numChildren; i++) {
    windows.push_back(childrenWindows[i]);

    auto childWindows = getWindows(display, windows[i]);
    windows.reserve(windows.size() + childWindows.size());
    windows.insert(windows.end(), childWindows.begin(), childWindows.end());
  }

  return windows;
}

std::vector<Window> getWindowsAbove(Display *display, Window rootWindow,
  Window window) {
  Atom pidAtom = XInternAtom(display, "_NET_WM_PID", true);
  Window rWindow;
  Window parentWindow;
  Window *childrenWindows;
  unsigned int numChildren;
  XQueryTree(display, rootWindow, &rWindow, &parentWindow,
    &childrenWindows, &numChildren);

  bool reachedWindow = false;
  std::vector<Window> windows = { window };
  for (int i = 0; i < numChildren; i++) {
    if (!reachedWindow) {
      if (childrenWindows[i] == window) {
        reachedWindow = true;
      }
      continue;
    }
    windows.push_back(childrenWindows[i]);

    auto childWindows = getWindows(display, childrenWindows[i]);
    windows.reserve(windows.size() + childWindows.size());
    windows.insert(windows.end(), childWindows.begin(), childWindows.end());
  }

  return windows;
}

std::vector<float> getWindowCoords(Display *display, Window window) {
  // Gather geometry of all windows in higher zorder
  std::vector<float> coords = {};

  Window rootWindow = DefaultRootWindow(display);
  auto windows = getWindows(display, rootWindow);
  std::vector<Window> higherOrderWindows =
    getWindowsAbove(display, rootWindow, window);
  for (auto window : higherOrderWindows) {
    bool viewable = Utils::windowIsViewable(display, window);
    if (viewable) {
      // Indicates whether a window is mapped or not
      XWindowAttributes windowAttributes;
      XGetWindowAttributes(display, window, &windowAttributes);
      coords.push_back(windowAttributes.x);
      coords.push_back(windowAttributes.y);
      coords.push_back(windowAttributes.width);
      coords.push_back(windowAttributes.height);
    }
  }

  return coords;
}

float calculateWindowOverlap(float x1, float y1, float w1, float h1,
  float x2, float y2, float w2, float h2, float x3 = 0, float y3 = 0,
  float w3 = 65535, float h3 = 65535) {
  return
    std::max(0.0f, std::min(std::min(x1+w1, x2+w2), x3+w3) -
      std::max(std::max(x1, x2), x3)) *
    std::max(0.0f, std::min(std::min(y1+h1, y2+h2), h3+h3) -
      std::max(std::max(y1, y3), y3));
}

namespace Utils {
  bool windowIsVisible(Display *display, Window window, float threshold) {
    // Indicates whether a window is fully covered
    if (!windowIsViewable(display, window)) {
      return false;
    }

    auto coords = getWindowCoords(display, window);
    float area = coords[2] * coords[3];
    float coveredArea = 0;

    // Calculate overlap of stacked-on-top windows
    for (int i = 4; i < coords.size(); i+=4) {
      coveredArea += calculateWindowOverlap(
        coords[0], coords[1], coords[2], coords[3],
        coords[i], coords[i+1], coords[i+2], coords[i+3]);
    }

    // Remove "double counting" of overlap
    for (int i = 4; i < coords.size(); i+=4) {
      for (int j = i+4; j < coords.size(); j+=4) {
        coveredArea -= calculateWindowOverlap(
          coords[0], coords[1], coords[2], coords[3],
          coords[i], coords[i+1], coords[i+2], coords[i+3],
          coords[j], coords[j+1], coords[j+2], coords[j+3]);
      }
    }

    float tol = 1e-4;
    return (1 - coveredArea/area + tol) >= threshold;
  }

  bool windowIsViewable(Display *display, Window window) {
    // Indicates whether a window is mapped or not
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
