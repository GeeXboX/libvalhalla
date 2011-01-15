#!/bin/sh

file=$1
name=$(echo $file | sed 's,.*/\(.*\),\1,')

i=0
for l in $(grep "VH_TEST (.*) {" "$file" | sed 's,.*(\(.*\)).*,\1,'); do
  st=$(grep -n "VH_TEST ($l) {" "$file" | sed 's,\(.*\):.*,\1,')
  en=$(grep -n "} VH_TEST ($l)" "$file" | sed 's,\(.*\):.*,\1,')
  if [ $i = 0 ]; then
    cat "$file" | sed -n $st,${en}p > "$name"
    i=1
  else
    cat "$file" | sed -n $st,${en}p >> "$name"
  fi
done
