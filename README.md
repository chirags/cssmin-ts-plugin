CSS Minify plugin for Apache Traffic Server

Derived from the append-transform example plugin
--

Usage
1) Check out Apache Traffic Server 2.x
2) Add css-transform-plugin to the SUBDIRS list in traffic/example/Makefile.am
3) Copy cssmin-ts-plugin  into traffic/example/
4) Add the following to traffic/configure.ac
   AC_CONFIG_FILES([example/css-transform/Makefile])

5) Add css-transform.so  /usr/local/etc/trafficserver/plugin.config
6) Make and install the cssmin-ts-plugin. make; sudo make install;
7) Launch traffic server. traffic_server  -p 9090

  
