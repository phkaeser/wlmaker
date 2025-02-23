<!--
Copyright 2023 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->
# Toolkit {#toolkit_page}

## Compositor Elements

### Class Hierarchy

<!-- See https://plantuml.com/class-diagram for documentation. -->
* Where do we use composition, vs. inheritance?

```plantuml
class Element {
  int x, y
  struct wlr_scene_node *node_ptr
  Container *parent_container_ptr

  bool init(handlers)
  bool init_attached(handlers, struct wlr_scene_tree *)
  void fini()
  -void set_parent_container(Container*)
  -void attach_to_scene_graph()
  void set_visible(bool)
  bool is_visible()
  void set_position(int, int)
  void get_position(int*, int*)
  void get_size(int*, int*)

  {abstract}#void destroy()
  {abstract}#struct wlr_scene_node *create_scene_node(parent_node*)
  #void pointer_motion(double, double)
  #void pointer_button(wlmtk_button_event_t)
  #void pointer_leave()
}
note right of Element::"set_parent_container(Container*)"
  Will invoke set_parent_container.
end note
note right of Element::"attach_to_scene_graph()"
  Will create or reparent this element's node to the parent's scene tree, or
  detach and destroy this element's node, if the parent does not have a scene
  tree.
end note

class Container {
  Element super_element
  Element elements[]

  bool init(handlers)
  void fini()
  add_element(Element*)
  remove_element(Element*)
  -struct wlr_scene_tree *wlr_scene_tree()

  {abstract}#void destroy()
  #void configure()
}
Element <|-- Container
note right of Element::"add_element(Element*)"
  Both add_element() and remove_element() will call
  Element::set_parent_container() and thus alignt he element's scene node
  with the container's tree.
end note

class Workspace {
  Container super_container
  Layer layers[]

  Container *create()
  void destroy()

  map_window(Window*)
  unmap_window(Window*)

  activate_window(Window*)
  begin_window_move(Window*)

  map_panel(Panel *, layer)
  unmap_panel(Panel *)
}
Container *-- Workspace

class Box {
  Container super_container

  bool init(handlers, wlmtk_box_orientation_t)
}
Container <|-- Box



abstract class Surface {
  Element super_element

  request_size()
  get_size()
}


abstract class Content {
  Container super_container

  Surface surface
  Surface popups[]

  init(surface)
  fini()

  request_size()
  get_size()

  request_close()
  set_activated()
}




class Toplevel {
  Content super_content

  -- because: implement request_close, set_activated, ...

  -- but: Window ?
}

class Popup {
  Surface super_surface
}



abstract class Content {
  Element super_element

  init(handlers)
  fini()
  struct wlr_scene_node *create_scene_node()
  Element *element()
  -set_window(Window*)

  {abstract}#void get_size(int *, int *)
  {abstract}#void set_size(int, int)
  {abstract}#void set_activated(bool)
  {abstract}#void set_maximized(bool)
  {abstract}#void set_fullscreen(bool)
}
Element <|-- Content
note right of Content
  Interface for Window contents.
  A surface (or... buffer? ...). Ultimately wraps a node,
  thus may be an element.
end note

class Layer {
  Container super_container


  add_panel()
  remove_panel()
}
Container <|-- Layer

class Panel {
  Element super_element

  {abstract}configure()
  #set_layer(Layer *)
}
Element <|-- Panel

class XdgToplevelSurface {
}
Content <|-- XdgToplevelSurface

class Buffer {
  Element parent

  init(handlers, struct wlr_buffer *)
  set(struct wlr_buffer *)
}
Element <|-- Buffer

class Button {
  Buffer super_buffer

  init(handlers, texture_up, texture_down, texture_blurred)
  update(texture_up, texture_down, texture_blurred)
}
Buffer <|-- Button

class Window {
  Box super_box
  Content *content
  TitleBar *title_bar

  Window *create(Content*)
  destroy()
  Element *element()

  set_activated(bool)
  set_server_side_decorated(bool)
  get_size(int *, int *)
  set_size(int, int)
}
Box *-- Window

class TitleBar {
  Box super_box
}
Box *-- TitleBar

class TitleBarButton {
  Button super_button

  init(handlers, overlay_texture, texture, posx, posy)
  redraw(texture, posx, posy)

  get_width(int *)
  set_width(int)
}

class Menu {
  Box super_box
}
Box *-- Menu

class MenuItem {

}
Buffer <|-- MenuItem

class Cursor {
  Cursor *create()
  void destroy()

  attach_input_device(struct wlr_input_device*)
  set_image(const char *
}

class LayerShell {
}
Panel *-- LayerShell
```


### Thoughts on windows, popups and menus

* Window: is a toplevel element (parent: workspace), may have decorations,
  and toplevel interactions (eg. close, maximize, ...).
  -> it's "body element" can be a surface, a menu (others? a legal info box?).
  => XDG and XWM toplevels correspond to a window.
     a "body element" may want to add a popup to the "parent"



* Popup: is child to a window or to another popup.
  -> it's body element can be a surface, a menu (others?)
     a "body element" may want to add a popup to the "parent"
  -> has a parent. Type of parent? a "pane" ? a "base" ?
  -> a popup *may* have partial decoration

* Menu: Can be the body of a window (root menu) or a popup (window menu).
  Is a submenu a popup to the menu or to the parent of the menu?
  (pro "to the menu": interactions are simpler (?)
   pro "to the parent": consistency <-- that's what it should be)

* Menu on right-click mode:
  - Will remain open until click is outside the menu
  - item remains highlighted *if pointer is on* OR *corresponding popup is open*

* there is common functionality between popup and window. eg. both can have
  further popups. Both have a child element. Have a common class from which
  both derive? or is "window" an extension to "popup" ?

Should "pane" be a pure virtual, or an instantiable class?
* an XDG popup: composed of a pane that wraps the surface, with the XDG sugar.
* an XWL popup: composed of a pane that wraps the surface, with XWL sugar.
* a menu popup: composed of a pane that wraps the menu box.
=> surface_pane and menu_pane?  (no functional difference?)
   (this is overlapping with current 'content'; but popup holds only 'element'.
    but, root menu should be a window


Window:
* request_fullscreen
  -> For surfaces, send async to clients. Needs specific implementation.
  -> For menu: ignore. Should be flagged as "not supported"
* request_maximized
  -> For surfaces, send async to clients. Needs specific implementation.
  -> For menu: ignore. Should be flagged as "not supported"
* request_size
  -> async call to content element to adjust size. For surfaces, will async
     send this to clients. (XWL, XDG) => needs specific implementation.
  -> For menu: ignore. Should be flagged as "not supported"
* request_close: request the content to close; will close the window.
  Needs specific implementation.
  -> for surfaces: relay back to the client.
  -> For menu: close the menu.
* set_activated: activates (kb focus)
  -> for surfaces: relay back to the client.
  -> for menu: update representation (local call).

Popup
* request_size: should be supported
* request_close: request just that window or popup to disappear (?)
* set_activated: activates (kb focus)

#### Menu
* pane: hold multiple popups, hold a child element.
* window: holds a "pane", decorations, link to workspace, ...
* popup: a pane, link to parent pane.

a window menu: when desiring to create a child menu
* the menu is an element to a pane. it looks up that pane.
* creates a new popup (pane?) and adds it to the parent pane.

a root menu: when desiring to create a child menu..
* the menu is an element to a pane. it looks up that pane.
* creates a new popup (pane?) and adds it to the parent pane.

the popup menu is closed
* it is closed when a click goes outside the popup menu (and outside
  any of it's sub menus. But, these are part of the pane?)
* it should know what parent item it was created for. signal there.

=> The menu *is* an implementation of the pane. It can extend it's handlers.
   it is a pane that (internally) holds a box, which holds menu items.
=> the menu can close once the pointer is no longer in any child element,
   of both pane and popup (panes).

#### A window or popup surface
* a pane that holds a surface as element.

### Pending work

* Separate the "map" method into "attach_to_node" and "set_visible". Elements
  should be marked as visible even if their parent is not "mapped" yet; thus
  leading to lazy instantiation of the node, once their parent gets "mapped"
  (ie. attached to the scene graph).

### User Journeys

#### Creating a new XDG toplevel

* xdg_toplevel... => on handle_new_surface

  * XdgToplevelSurface::create(wlr surface)
    * listeners for map, unmap, destroy

      => so yes, what will this do when mapped?

  * Window::create(surface)
    * registers the window for workspace

    * creates the container, with parent of window element
    * if decoration:


  * will setup listeners for the various events, ...
    * request maximize
    * request move
    * request show window menu
    * set title
    * ...


  set title handler:

  * window::set_title

  request maximize handler:
  * window::request_maximize
    * window::set_maximized
      * internally: get view from workspace, ... set_size
      * callback to surface (if set): set_maximized


  upon surface::map

  * workspace::add_window(window)   (unsure: do we need this?)
    => should set "container" of window parent... element to workspace::container
    (ie. set_parent(...); and add "element" to "container")

  * workspace::map_window(window)
    => this should add window to the set of workspace::mapped_windows
    => window element->container -> map_element(element)
      (expects the container to be mapped)

    => will call map(node?) on window element
      - is implemented in Container:
      - create a scene tree (from parents node) oc reparent (from parent)
      - calls map for every item in container

  upon surface::unmap
  * workspace::unmap_window

    => window element->container -> unmap_element(element)
    => will call unmap() on window element
      => destroy the node

  * workspace::remove_window(window)   (do we need this?)


There is a click ("pointer button event") -> goes to workspace.

  * use node lookup -> should give data -> element
  * element::click(...)
  * Button::click gets called. Has a "button_from_element" & goes from there.


Button is pressed => pass down to pointer-focussed element.
  Would eg. show the "pressed" state of a button, but not activate.

  button_down

Button is released => pass down to pointer-focussed element.
  (actually: pass down to the element where the button-press was passed to)
  Would eg. acivate the button, and restore the state of a pressed button.

  button_up
  click

Button remains pressed and pointer moves.
  Means: We might be dragging something around.
  Start a "drag" => pass down a "drag" event to pointer focussed element.
  Keep track of drag start, and pass on relative drag motion down to element.
  Keeps passing drag elements to same element until drag ends.
  Would keep the element pointer focussed (?)

  A 'button' would ignore drags. drag_begin, drag_end, drag_motion ?
  A 'titlebar' would use this to begin a move, and update position.
  A 'iconified' would use this to de-couple from eg. dock

  drags have a pointer button associated (left, middle, right),
  and a relative position since. They also have the starting position,
  relative to the element.

  button_down
  [lingering time, some light move]
  drag_begin
  drag_motion
  drag_motion
  button_up
  drag_end

Button is pressed again, without much move since last press.
  Means: We have a double-click.
  Pass down a double-click to the pointer-focussed element.

  button_down
  button_up
  double_click


## Dock and Clip class elements

```plantuml
class DockOptions {
  int anchor
  int orientation
  int layer
}

class Dock {
  Panel super_panel

  init(DockOptions options)

  Tile entries[];

  add_entry()
  remove_entry()
}

class Tile {
  Element

  set_size(int size)
}

class Launcher {

  const char *icon_file_path;
  const char *commandline;
}
Tile <|-- Launcher

class Icon {}
Tile <|-- Icon

class IconSurface {}
Tile <|-- IconSurface

class Clip {}
Dock <|-- Clip

class IconArea {}
Dock <|-- IconArea
```

### Description

* A `Dock` is the base class for the Dock, Clip or icon area. It has an anchor
  to either a corner or an edge of the screen, and an orientation (vertical or
  horizontal). On screen edges, the orientation must be parallel to the edge's
  orientation.

* A `Tile` is the parent for what's shown in the dock, clip or the icon
  area. An entry is quadratic, and the size is given by the dock. The size may
  change during execution.
  A Tile will accept and may pass on pointer events.

* A `Launcher` is an implementation of a Tile.
  It shows an image (the application icon), and will spawn a subprocess to
  execute the configured commandline when invoked.
  A launcher is invoked by a click (TODO: doubleclick?).
  It shows status of the spawned subprocesses ("running", "starting", "error").

  If the application is running, it may show the application's icon (provided by
  the running application via a Wayland protocol), instead of the pre-configured
  one.

  The Launcher is not part of wlmtk, but of wlmaker implementation.

### Thoughts

* A running application in WLMaker may (1) have an icon, and (2) be in
  miniaturized (*iconified*) state.

  An "application" in this context refers to ... a wayland client? an XDG
  toplevel? Any toplevel (eg. X11 toplevel)? For UI interaction, a "toplevel"
  seems a good answer, as it's the *toplevel* that can be *iconified*.
  That would apply to both X11 and XDG shell toplevels.

  Implication: WLMaker needs to keep track of "applications".

  If there is an icon, it should be shown on the launcher that spawned the
  "application". (showing the icon of the most recently launched one).
  Otherwise, it should be displayed in the icon area.

  An "application" that is *iconified* will be shown in the icon area. This is
  irrespective of whether there is already an icon shown for that "application".

### Input grab

* See: https://wayland-book.com/seat.html
* See: https://wayland.app/protocols/xdg-shell#xdg_popup:request:grab

So, when a XDG popup requests a grab: From that moment on, the coresponding
wlr_surface (and the related client) should keep receiving events. But not
others. Once the grab is broken, the popup is supposed to be dismissed.

So far, we been thinking of passing events from root element along the
containers. On a grab, each container would lock the 'grabbing' element.
(and inform the grab-holder when another element claims the grab; so
would need a cancel_gab method).

When the menu requests grab: we also want all pointer and input events
going there. When the grab is broken => menu is to close.

container_grab(c, element)  -> setup grab for element
  -> will call to parent container as container_grab(parent_c, c.super_element)
element_grab_cancel(element) -> cancel a held grab (this is FYI)


void *wlmtk_container_pointer_grab(wlmtk_container_t *, wlmtk_element_t *);
void wlmtk_container_pointer_grab_release(wlmtk_container_t *, wlmtk_element_t *);
void wlmtk_element_pointer_grab_cancel(wlmtk_element_t *element_ptr);


For Keyboard:
* we have that mechanism partly with container::keyboard_focus_element_ptr
* we have keyboard routing through "set_keyboard_focus_element
  (through wlmtk_surface_t in wlmtk_surface_:set_activated)

For Pointer or Touch:
* Not done (yet).

=> HOWEVER: This will route *only* to the surface holding the grab.
   (this would prevent cursor updates? That's actually how X11 chrome
    popups/menus are working currently)
   so... that's probably good/desired.
