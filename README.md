# MIDI Studio UI (ms-ui)

Shared LVGL UI primitives/components for MIDI Studio targets:

- standalone firmware/app (`midi-studio/core`)
- DAW plugins firmware UIs (ex: `midi-studio/plugin-bitwig`)

This repo is product-level (not OpenControl). It intentionally keeps DAW-specific logic and themes out.

## Tests locaux

Prérequis : CMake 3.29+, compilateurs C/C++20, Ninja, checkout OpenControl
(`framework`, `ui-lvgl`, `ui-lvgl-components`) et sources LVGL à la révision du
produit. Aucun téléchargement implicite. Depuis la racine de ce dépôt :

```sh
cmake -S . -B build/tests -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DMS_UI_OPEN_CONTROL_ROOT=/chemin/open-control -DMS_UI_LVGL_DIR=/chemin/lvgl
cmake --build build/tests --parallel 8
ctest --test-dir build/tests --output-on-failure
```

Avec Zig sous Windows, ajouter à la configuration
`-DCMAKE_C_COMPILER=/chemin/zig-cc.cmd` et
`-DCMAKE_CXX_COMPILER=/chemin/zig-cxx.cmd` (wrappers du workspace).

CTest enregistre `test_CurvePreviewGeometry` et `test_VirtualListOverlay`.
Ce dernier utilise un écran LVGL en mémoire avec sa propre configuration :
première frame, transitions, lignes recyclées, sparkline et cycle de vie du
`ListOverlay` partagé. Les assertions restent actives en Release. Les polices
LVGL intégrées remplacent les assets : ce test ne clôt pas la comparaison
visuelle des applications Core/Bitwig. La CI exécute les deux cibles en Release
(assertions actives), sur chaque PR et sur `main`, avec les dépendances épinglées
dans `.github/workflows/ci.yml`.
