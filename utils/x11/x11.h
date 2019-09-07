#pragma once

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#include <vector>
#include <string>

namespace Utils {
  bool windowIsVisible(Display *display, Window window, float threshold);
  bool windowIsViewable(Display *display, Window window);
  void findWindowsOwnedByExeRecursive(Display *display, std::string exeName,
    bool useCmdLine, Window startWindow, std::vector<Window> &windows);
  std::vector<Window> findWindowsOwnedByExe(Display *display, Window rootWindow,
    std::string exeName, bool useCmdLine);
}