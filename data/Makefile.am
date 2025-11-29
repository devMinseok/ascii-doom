
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
icons_DATA = @PROGRAM_PREFIX@doom.png    \
             @PROGRAM_PREFIX@heretic.png \
             @PROGRAM_PREFIX@hexen.png   \
             @PROGRAM_PREFIX@setup.png   \
             @PROGRAM_PREFIX@strife.png

@PROGRAM_PREFIX@doom.png :
	@if test -f $(top_srcdir)/data/doom.png; then \
		cp $(top_srcdir)/data/doom.png $@; \
	else \
		touch $@; \
	fi

@PROGRAM_PREFIX@heretic.png :
	@if test -f $(top_srcdir)/data/heretic.png; then \
		cp $(top_srcdir)/data/heretic.png $@; \
	else \
		touch $@; \
	fi

@PROGRAM_PREFIX@hexen.png :
	@if test -f $(top_srcdir)/data/hexen.png; then \
		cp $(top_srcdir)/data/hexen.png $@; \
	else \
		touch $@; \
	fi

@PROGRAM_PREFIX@setup.png :
	@if test -f $(top_srcdir)/data/setup.png; then \
		cp $(top_srcdir)/data/setup.png $@; \
	else \
		touch $@; \
	fi

@PROGRAM_PREFIX@strife.png :
	@if test -f $(top_srcdir)/data/strife.png; then \
		cp $(top_srcdir)/data/strife.png $@; \
	else \
		touch $@; \
	fi

CLEANFILES = $(icons_DATA)
