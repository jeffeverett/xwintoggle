UTILS = utils/config/config.cpp utils/x11/x11.cpp
UTILHEADERS = utils/config/config.h utils/x11/x11.h

xwintoggle: main.cpp $(UTILS) $(UTILHEADERS)
	g++ -o xwintoggle -g main.cpp $(UTILS) -lX11 -lyaml-cpp -I .