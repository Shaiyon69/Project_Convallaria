# Convallaria (C++ / raylib)

C++ port of the Godot game. It reads its assets from the repo root, so the Godot asset folders must stay where they are.

```sh
cmake -S . -B build -G Ninja      # downloads raylib 5.5 the first time
cmake --build build
./build/convallaria               # --test runs the self-check
```

Controls: WASD or arrow keys to move, Esc to pause, 1/2/3 or mouse to pick upgrades.
