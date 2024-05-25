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
# Configuration Files {#conf_page}

Configuration files are in *text plist* (or: *ASCII plist*) format:
https://code.google.com/archive/p/networkpx/wikis/PlistSpec.wiki

<!-- See https://plantuml.com/class-diagram for documentation. -->
```plantuml

class Object {
  {abstract}#void destroy()
}

class Dict {
  Object get(String key);

  #void destroy()
}
Dict <|-- Object

class Array {
  size_t size()
  Object get(size_t idx)
  #void destroy()
}
Array <|-- Object

class String {
  const char *get_value();

  #void destroy()
}
String <|-- Object

class DictItem {

  -String key
  -Object value
}

class ArrayElement {
  Object value
}

```

