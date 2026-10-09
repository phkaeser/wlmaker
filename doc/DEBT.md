# Backlog of technical debt

## src/wlclient

* Remove the dual method for defining interfaces.
* Each bound interface should be tracked by it's name.
* The "register" method should provide an "unregister" method, too.

## tool/toplevel_list

* Change the tool's name to a more generic WaylandListener
* Provide sub-tools, eg. for listing to output, toplevels, ...
