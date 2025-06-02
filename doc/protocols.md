# Wayland Protocol support {#protocols}

# Wayland Protocol support

This page documents which of the Wayland protocols (beyond the core
[Wayland protocol](https://wayland.app/protocols/wayland)) are supported
by wlmaker.

# Idle inhibit

Permits inhibiting the idle behaviour, such as locking the screen.

* Status: Implemented, untested.
* Reference: https://wayland.app/protocols/idle-inhibit-unstable-v1

# Session lock

Allows privileged Wayland clients to lock the session and display arbitrary
graphics while the session is locked. An example of such a client is
[swaylock](https://github.com/swaywm/swaylock).

* Status: Supported.
* Reference: https://wayland.app/protocols/ext-session-lock-v1

# wlr layer shell {#protocols_wlr_layer_shell}

Clients can use this interface to associate `wl_surface` with a *layer* of the
output. The surfaces may slo be anchored to edges and/or corners of a screen,
specify input semantics and will be rendered above or below desktop elements,
as configured.

* Status: Supported, except popups and input semantics.
* Reference: https://wayland.app/protocols/wlr-layer-shell-unstable-v1

# wlr screencopy

This protocol allows clients to ask the compositor to copy part of the screen
content to a client buffer.

* Status: Supported.
* Reference: https://wayland.app/protocols/wlr-screencopy-unstable-v1

# wlr output management {#protocols_wlr_output_management}

Implements interfaces to obtain and modify output device configruation. It
permits thirdparty clients (such as `wlr-randr`, `wdisplays`) to obtain a list
of available outputs, and set their propertis.

* Status: Supported.
* Reference: https://wayland.app/protocols/wlr-output-management-unstable-v1

# XDG decoration

This interface allows a compositor to announce support for server-side
decorations. A client can use this protocol to request being decorated by
wlmaker.

* Status: Supported.
* Reference: https://wayland.app/protocols/xdg-decoration-unstable-v1

# XDG shell

Interface for creating desktop-style surfaces.

* Status: Supported, with a few gaps.
* Reference: https://wayland.app/protocols/xdg-shell
