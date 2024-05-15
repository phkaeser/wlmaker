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
class Dock {
  Container container[]
  DockEntry entries[]
}

class DockEntry {
  Element
}

class Launcher {
}
DockEntry <|-- Launcher

class Icon {}
DockEntry <|-- Icon

class IconSurface {}
DockEntry <|-- IconSurface

class Clip {}
Dock <|-- Clip

class IconArea {}
Dock <|-- IconArea
```

