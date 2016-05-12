# Packaging a Juno game for distribution

To package your game for distribution, you should create a zip archive containing the game's source code and assets. The game's `main.lua` file should be at the root of this archive.

The zip file should be renamed to `pak0` (with no extension) and placed in the same directory as the Juno executable. When Juno runs it will search for the `pak0` file and load it if it exists.

The dynamically linked libraries should also be included when distributing your game. On Windows these are `SDL.dll` and `lua51.dll`. The Juno executable can be renamed to the title of your game. This should result in the following files:

```
game_title.exe  (juno executable)
pak0            (zip archive of game)
lua51.dll
SDL.dll

```
