# The teaser trailer

`trailer.json` says everything about it; `make_trailer.py` makes it.

```
python engine/tools/trailer/make_trailer.py              # build/nexgen-trailer.mp4, at 1920x1080
python engine/tools/trailer/make_trailer.py --preview    # half the size, quickly, for trying things
```

Needs ffmpeg and Python 3 with Pillow. Each shot is made once and kept in `build/trailer-shots`; after a change only the
shots that changed are made again, and then they are put together with the music.

## Changing it

- **Words**: `title` and `sub` in the middle, `caption` and `captionSub` at the bottom left, `mark` for words made to look
  like the logo (REDUX). `textIn` and `textOut` are when they come and go, in beats from the start of the shot.
- **Timing**: shots last so many `beats` of the music (107.67 to the minute), so cuts land on the beat. Changing one
  moves those after it. The first lasts `to` a time, to start the rest on a bar.
- **Footage**: `clip` is a video, from `footage` (`build/trailer/footage`) or a path; `from` is where in it to start,
  `speed` 0.5 for slow motion, `zoom` [start, end] for a push in or out, `focus` [x, y] to keep a corner (a panel, say) out
  of the shot, `fit` to show all of a 4:3 picture within the letterbox.
- **How each comes in**: `in` is `cut`, `flash`, `fade`, `glitch`, `zoom`, `slide`, `circle`, `dissolve`, `blur` or
  `wipe`; `inBeats` how long it takes.
- **Look**: `grade` per shot (`vivid`, `dark`, `mono`, `night`, `warm`, `cold`); `look` for the letterbox, grain and
  vignette of all of it.
- **Music**: `parts` are the pieces of the track that are used, one after another; the default is its first 67 seconds,
  then its ending. `fadeIn` and `fadeOut` are seconds for it to come in softly and to go.

The footage is recorded from the engine with `engine/tools/record/record.py`, at 1080p and without the demos'
captions:

```
set RECORD_SIZE=1920x1080
set RECORD_CLEAN=1
python engine/tools/record/record.py <nexgen.exe> build/trailer/footage/forest.mp4 20 forest
```

The original Xbox shots come from `build/nexgen-demos-xbox.mp4`, captured from the console; its own counter at the top of
the picture shows the frame rate.

## The sequel: what's new

`whatsnew.json` is the second trailer, on usefulpix's retro synthwave, showing what was added since the first. It is
written by `whatsnew.py`, which lays each chapter on a section of the music and checks that it ends on that section's
edge: change the shots there and run it, then make it as above with `whatsnew.json`. Its footage is in
`build/trailer2/footage`, recorded the same way; `og-` clips are from the original Xbox, captured with the Elgato.
