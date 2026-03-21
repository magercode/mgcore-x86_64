#!/usr/bin/env sh
set -eu

VERSION="${1:-v10.x-binary}"
DESTINATION="${2:-tools/limine}"

if [ -d "${DESTINATION}" ]; then
  echo "Limine directory already exists at ${DESTINATION}"
  echo "Remove it first if you want to re-clone a different version."
  exit 0
fi

mkdir -p "$(dirname "${DESTINATION}")"
git clone https://codeberg.org/Limine/Limine.git --branch="${VERSION}" --depth=1 "${DESTINATION}"
echo "Limine binary release cloned to ${DESTINATION}"
echo "Re-run: cmake -S . -B build -G Ninja"
