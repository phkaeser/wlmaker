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
  void fini()
  -void set_parent_container(Container*)
  -void attach_to_scene_graph()
  void set_visible(bool)
  bool is_visible()
  void set_position(int, int)
  void get_position(int*, int*)

  {abstract}#void destroy()
  {abstract}#struct wlr_scene_node *create_scene_node(parent_node*)
  {abstract}#void enter()
  {abstract}#void leave()
  {abstract}#void click()
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

  void enter(Element*)
  void leave(Element*)
  void click(Element*)
}
Element <|-- Container
note right of Element::"add_element(Element*)"
  Both add_element() and remove_element() will call
  Element::set_parent_container() and thus alignt he element's scene node
  with the container's tree.
end note

class Workspace {
  Container super_container
  Container layers[]

  Container *create()
  void destroy()

  map_window(Window*)
  unmap_window(Window*)

  map_layer_element(LayerElement *, layer)
  unmap_layer_element(LayerElement *, layer)
}
Container *-- Workspace

class HBox {
  Container parent
}
Container <|-- HBox

class VBox {
  Container parent
}
Container <|-- VBox

abstract class Content {
  Element parent

  init(handlers)
  fini()
  struct wlr_scene_node *create_scene_node()
  Element *element()
  -set_window(Window*)

  {abstract}#void set_active(bool)
  {abstract}#void set_maximized(bool)
  {abstract}#void set_fullscreen(bool)
  {abstract}#void enter(Element*)
  {abstract}#void leave(Element*)
  {abstract}#void click(Element*)
}
Element <|-- Content
note right of Content
  Interface for Window contents.
  A surface (or... buffer? ...). Ultimately wraps a node,
  thus may be an element.
end note

class LayerElement {
  Element parent

  {abstract}#void enter(Element*)
  {abstract}#void leave(Element*)
  {abstract}#void click(Element*)

  {abstract}#configure()
  }
Element <|-- LayerElement

class LayerShell {
}
LayerElement <|-- LayerShell

class XdgToplevelSurface {
}
Content <|-- XdgToplevelSurface

class Buffer {
  Element parent
}
Element <|-- Buffer

class Button {

}
Buffer <|-- Button

class Window {
  VBox super_container
  Content *content
  TitleBar title_bar

  Window *create(Content*)
  destroy()
  Element *element()
}
VBox *-- Window

class TitleBar {
  HBox parent
}
HBox *-- TitleBar

class Menu {
  VBox parent
}
VBox *-- Menu

class MenuItem {

}
Buffer <|-- MenuItem
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

