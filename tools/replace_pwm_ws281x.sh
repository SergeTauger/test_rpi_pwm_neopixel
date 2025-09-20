#!/usr/bin/env bash
# replace_pwm_sed.sh
# Usage: ./replace_pwm_sed.sh [directory]   (defaults to current directory)

DIR="${1:-.}"

# Enable recursive globbing (bash 4+)
shopt -s globstar nullglob

# Loop over all .c and .h files under $DIR
for file in "$DIR"/**/*.c "$DIR"/**/*.h; do
    # Skip if the glob didn't match anything
    [[ -e "$file" ]] || continue

    echo "Processing: $file"
    # In‑place edit with a backup (.bak)
    sed -i.bak -E 's/\b(pwm_t)\b/ws281x_pwm_t/g' "$file"
done
