#! /bin/sh
# Generates Cscope index for wlmaker and all dependencies.

set -o errexit

SUBPATHS="\
dependencies \
src \
submodules"

base_path="$(readlink -f "$(dirname "${0}")")"
rm -f "${base_path}/cscope.files"

for p in ${SUBPATHS} ; do
    echo "Processing ${base_path}/${p} ..."
    find "${base_path}/${p}" -name "*.h" -o -name "*.c" -o -name "*.cpp" | xargs etags
    find "${base_path}/${p}"  -name "*.h" -o -name "*.c" -o -name "*.cpp"  >> cscope.files
done

cscope -Rbkq -i cscope.files 2>/dev/null
echo "Done."
exit 0
