
EXTRA_DIST=              \
        README           \
        doom.ico         \
        doom.png         \
        heretic.ico      \
        heretic.png      \
        hexen.ico        \
        hexen.png        \
        setup.ico        \
        setup.png        \
        strife.ico       \
        strife.png       \
        wadfile.png      \
        convert-icon

iconsdir = $(prefix)/share/icons/hicolor/128x128/apps

# For Emscripten builds, skip hexen.png and strife.png if they don't exist
@ENABLE_EMSCRIPTEN_TRUE@icons_DATA = @PROGRAM_PREFIX@doom.png    \
@ENABLE_EMSCRIPTEN_TRUE@             @PROGRAM_PREFIX@heretic.png \
@ENABLE_EMSCRIPTEN_TRUE@             @PROGRAM_PREFIX@setup.png
@ENABLE_EMSCRIPTEN_FALSE@icons_DATA = @PROGRAM_PREFIX@doom.png    \
@ENABLE_EMSCRIPTEN_FALSE@             @PROGRAM_PREFIX@heretic.png \
@ENABLE_EMSCRIPTEN_FALSE@             @PROGRAM_PREFIX@hexen.png   \
@ENABLE_EMSCRIPTEN_FALSE@             @PROGRAM_PREFIX@setup.png   \
@ENABLE_EMSCRIPTEN_FALSE@             @PROGRAM_PREFIX@strife.png

@PROGRAM_PREFIX@doom.png : doom.png
	cp $(top_srcdir)/data/doom.png $@

@PROGRAM_PREFIX@heretic.png : heretic.png
	cp $(top_srcdir)/data/heretic.png $@

@ENABLE_EMSCRIPTEN_FALSE@@PROGRAM_PREFIX@hexen.png : hexen.png
@ENABLE_EMSCRIPTEN_FALSE@	cp $(top_srcdir)/data/hexen.png $@

@PROGRAM_PREFIX@setup.png : setup.png
	cp $(top_srcdir)/data/setup.png $@

@ENABLE_EMSCRIPTEN_FALSE@@PROGRAM_PREFIX@strife.png : strife.png
@ENABLE_EMSCRIPTEN_FALSE@	cp $(top_srcdir)/data/strife.png $@

CLEANFILES = $(icons_DATA)
