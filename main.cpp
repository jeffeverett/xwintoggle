#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <iostream>

int main(int argc, char *argv [])
{
  Display *display;
  XEvent event;
  int screen_num, done;

  // Open connection to the server
  display = XOpenDisplay("");
  Window rootWindow = DefaultRootWindow(display);

  // Grab relevant keyboard shortcuts
  int f1Keycode = XKeysymToKeycode(display, XK_F1);
  int f2Keycode = XKeysymToKeycode(display, XK_F2);
  XGrabKey(display, f1Keycode, AnyModifier, rootWindow, false, GrabModeAsync, GrabModeAsync);
  XGrabKey(display, f2Keycode, 0, rootWindow, false, GrabModeAsync, GrabModeAsync);


  // Enter the event loop
  done = 0;
  while(done == 0)
  {
    XNextEvent(display, &event);
    std::cout << "event of type " << event.type << std::endl;
    switch(event.type) {
      case KeyPress:
        if (event.xkey.keycode == f1Keycode) {
          std::cout << "F1 pressed" << std::endl;
        } else if (event.xkey.keycode == f2Keycode) {
          std::cout << "F2 pressed" << std::endl;
        }

        break;
    }
  }

	// Clean up before exiting
  XCloseDisplay(display);
}