#include "utils/x11/x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <math.h>

#define DEBUG 0

void printWindowDebug(unsigned long pid, Window window) {
  #ifdef DEBUG
    std::cout << "found window with PID " << pid;
    std::ios_base::fmtflags f(std::cout.flags());
    std::cout << " and ID 0x" << std::hex <<
      window << "." << std::endl;
    std::cout.flags(f);
  #endif
}

std::vector<std::vector<int>> getWindowCoords(Display *display,
  Window queryWindow, Window targetWindow,
  bool *reachedTargetPtr = nullptr, int absX = 0, int absY = 0) {
  // Gather geometry of all windows in higher zorder
  std::vector<std::vector<int>> coords = {};

  bool reachedTarget = false;
  if (!reachedTargetPtr) {
    reachedTargetPtr = &reachedTarget;
  }

  Window rWindow;
  Window parentWindow;
  Window *childrenWindows;
  unsigned int numChildren;
  XQueryTree(display, queryWindow, &rWindow, &parentWindow,
    &childrenWindows, &numChildren);

  for (int i = 0; i < numChildren; i++) {
    if (childrenWindows[i] == targetWindow) {
      *reachedTargetPtr = true;
    }

    XWindowAttributes windowAttributes;
    XGetWindowAttributes(display, childrenWindows[i], &windowAttributes);
    if (*reachedTargetPtr && windowAttributes.map_state == IsViewable &&
      windowAttributes.c_class != InputOnly) {
      coords.push_back(std::vector<int> {
        windowAttributes.x + absX,
        windowAttributes.y + absY,
        windowAttributes.x + absX + windowAttributes.width,
        windowAttributes.y + absY + windowAttributes.height });
    }

    if (childrenWindows[i] != targetWindow) {
      auto childCoords = getWindowCoords(display, childrenWindows[i],
        targetWindow, reachedTargetPtr, absX + windowAttributes.x,
        absY + windowAttributes.y);

      coords.reserve(coords.size() + childCoords.size());
      coords.insert(coords.end(), childCoords.begin(), childCoords.end());
    }
  }

  return coords;
}

int calculateWindowOverlap(std::vector<std::vector<int>> windowCoords) {
  if (windowCoords.size() == 0) {
    return 0;
  }

  #ifdef DEBUG
      std::cout << windowCoords.size() << std::endl;
      for (auto windCoords : windowCoords) {
        for (auto elem : windCoords) {
          std::cout << elem << ",";
        }
        std::cout << std::endl;
      }
  #endif

  std::vector<int> intersect = windowCoords[0];
  for (int i = 1; i < windowCoords.size(); i++) {
    intersect[0] = std::max(intersect[0], windowCoords[i][0]);
    intersect[1] = std::max(intersect[1], windowCoords[i][1]);
    intersect[2] = std::min(intersect[2], windowCoords[i][2]);
    intersect[3] = std::min(intersect[3], windowCoords[i][3]);
  }
  return std::max(0, intersect[2]-intersect[0]) *
    std::max(0, intersect[3]-intersect[1]);
}

namespace Utils {
  bool windowIsVisible(Display *display, Window window, float threshold) {
    // Indicates whether a window is fully covered
    if (!windowIsViewable(display, window)) {
      return false;
    }

    auto rootWindow = DefaultRootWindow(display);
    auto coords = getWindowCoords(display, rootWindow, window);

    if (coords.size() <= 1) {
      return true;
    }

    float area = (coords[0][2]-coords[0][0]) * (coords[0][3]-coords[0][1]);
    float coveredArea = 0;

    auto selector = std::vector<bool>(coords.size()-1);
    for (int i = 0; i < selector.size(); i++) {
      std::fill(selector.begin(), selector.begin()+i+1, true);
      std::fill(selector.begin()+i+1, selector.end(), false);

      auto selectedWindows = std::vector<std::vector<int>>(i);
      do {
        selectedWindows.clear();
        for (int j = 0; j < selector.size(); j++) {
          if (selector[j]) selectedWindows.push_back(coords[j+1]);
        }
        selectedWindows.push_back(coords[0]);
        coveredArea += pow(-1, i)*calculateWindowOverlap(selectedWindows);
      } while (std::prev_permutation(selector.begin(), selector.end()));
    }

    float tol = 1e-4;
    #ifdef DEBUG
      for (auto windCoords : coords) {
        for (auto elem : windCoords) {
          std::cout << elem << ",";
        }
        std::cout << std::endl;
      }
      std::cout << "Area: " << area << ", CoveredArea: " << coveredArea
        << std::endl;
    #endif
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
