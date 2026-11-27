# Nexgen Cook

Makes images, sounds and 3D models ready for the Nexgen engine. `nexgen-cook` is the window; `cook` is the command that does the
work, and the two must stay together. Nothing needs to be installed.

| From | What becomes of it |
|---|---|
| glTF (`.glb`, `.gltf`), FBX, OBJ, STL, PLY | A model that every platform reads, with its pictures inside |
| PNG, JPEG, TGA, BMP, GIF | A compressed texture |
| WAV, Ogg, MP3, FLAC | A sound the engine plays: short ones ADPCM, which costs little to play, and long ones Ogg; or all Ogg, WAV or ADPCM, as chosen |

Files keep their names, so scripts don't change; one file cooked on its own is named for what it has become, so `cook -a wav
tune.ogg` makes `tune.wav`, and `cook song.mp3` makes `song.ogg`. The engine plays WAV, Ogg and ADPCM, so MP3 and FLAC are
always made into one of those. Only what has changed is done again.

## The window

Drop a folder or a file on it, choose the platform and press Cook. With "Again as files change" it goes on cooking as you work.

## The command

```
cook --platform xboxog media media-xbox      a folder
cook house.fbx                               one model, as house.glb
cook --sounds-as ogg loop.wav                one sound, as loop.ogg
cook --help                                  everything else
```

`--json` writes a line of JSON for each file, for other programs. It ends with 0 if all went well, 1 if something failed and 2 if it
was asked wrongly.
