# Using valgrind for wlmaker

By default, valgrind's leak detection reports many potential and definitive
leaks from libdrm and from fontconfig (via cairo). Since these are outside
our control, it's helpful to suppress these leaks [^1].

To run with these leak-checks suppressed:
```
valgrind \
  --leak-check=full \
  --suppressions=./libdrm.supp \
  --suppressions=./libcairo-fontconfig.supp \
  build/src/wlmaker
```

## Generating suppressions files

To (re)generate suppressions, run:

```
valgrind \
  --leak-check=full \
  --show-reachable=yes \
  --error-limit=no \
  --gen-suppressions=all \
  --suppressions=./libdrm.supp \
  --suppressions=./libgallium.supp \
  --suppressions=./libcairo-fontconfig.supp \
  --log-file=new_suppressions.log \
  build/src/wlmaker
```

[^1]: https://wiki.wxwidgets.org/Valgrind_Suppression_File_Howto
