/* ========================================================================= */
/**
 * @file toolkit.h
 *
 * @copyright
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @page toolkit_page Toolkit Page
 *
 * Work in progress ...
 *
 * * Where do we compose, vs. inherit?
 *
 * @startuml

   class Element {
     int x, y
     struct wlr_scene_node *node_ptr
     Container *container_ptr

     Element *create(void)
     void destroy(Element*)
     set_parent(Container*)
     map()
     unmap()

     {abstract}#create_node()
   }
   note right of Element::"map()"
     Only permitted if element is a member of a (mapped?) container.
   end note

   class Container {
     Element parent
     Element children[]

     Container *create(void)
     void destroy(Container*)
     add_element(Element*)
     remove_element(Element*)

     map()
     unmap()
   }
   Element <|-- Container

   class Workspace {
     Container parent

     Container *create(void)
     void destroy(Container*)
   }
   Container <|-- Workspace

   class HBox {
     Container parent
   }
   Container <|-- HBox

   class VBox {
     Container parent
   }
   Container <|-- VBox

   class Surface {
     Element parent
   }
   Element <|-- Surface

   class Buffer {
     Element parent
   }
   Element <|-- Buffer


   class Workspace {
     Container parent

     Workspace *create(void)
     void destroy(Workspace*)
   }



   class Window {
     VBox parent
     Surface surface
     TitleBar title_bar
   }
   VBox <|-- Window

   class TitleBar {
     HBox parent
   }
   HBox <|-- TitleBar


 * @enduml
 */
#ifndef __TOOLKIT_H__
#define __TOOLKIT_H__

#include "gfxbuf.h"
#include "primitives.h"
#include "style.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus



#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __TOOLKIT_H__ */
/* == End of toolkit.h ===================================================== */
