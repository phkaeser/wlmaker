# Root Menu {#root_menu}

# Root Menu configuration file {#root_menu_file}

wlmaker displays a root menu when right-clicking on an unoccupied area of the
workspace, or when pressing the configured hotkey for the `RootMenu` action.

It uses a built-in onfiguration (see @ref etc_root_menu_plist), but can be
overriden by setting the `--root_menu_file` argument (see @ref commandline).

The root menu file must define a Plist array. The first element of the array
specifies the menu's title, and further elements will either define a menu
item, or a command to include a further plist file, or to invoke a shell
command to generate a plist menu output.

## Example: A menu with two items

@include tests/data/menu.plist

## Example: A menu including another file

@include tests/data/menu-include.plist

## Example: A menu invoking a shell command to generate the menu

@include tests/data/menu-generate.plist

## Automatic Generation

Wayland Maker provides the `wlmtool` utility for generating a Plist menu
specification from the applications found in the host's XDG application
repository.

This can be used to define a menu item with a submenu, or to directly define
the toplevel root window.

Example usage:
@snippet{trimleft} etc/RootMenuDebian.plist wlmtool

Alternatively, the [`wmmenugen`](https://www.windowmaker.org/docs/manpages/wmmenugen.html) tool from Window Maker can be used.
