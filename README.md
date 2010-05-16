# CSS Minify plugin for Apache Traffic Server
<p>Derived from the append-transform example plugin</p>

## Usage
- Check out Apache Traffic Server 2.x
- Add css-transform-plugin to the SUBDIRS list in traffic/example/Makefile.am
- Copy cssmin-ts-plugin  into traffic/example/
- Add the following to traffic/configure.ac
>   AC_CONFIG_FILES([example/css-transform/Makefile])

- Add css-transform.so  /usr/local/etc/trafficserver/plugin.config
- Make and install the cssmin-ts-plugin.
> make; sudo make install;
- Launch traffic server.
> traffic_server  -p 9090

  
