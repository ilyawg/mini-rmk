CC = gcc
CFLAGS = -march=c3-2 -O2 -pipe -mfpmath=sse,387 -msse2 -mmmx -msse
prefix = /usr/local
exec_prefix = ${prefix}
BINDIR = ${exec_prefix}/bin
DEFS =

INCLUDES = -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/freetype2 -I/usr/include/libpng12 -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include
PROGRAM = mini-rmk
SOURCES = main.c \
          protocol.c \
          commands.c \
          kkm.c \
          process.c \
          actions.c \
          conf.c \
          main_window.c \
          receipt.c \
          numeric.c \
          sqlite_common.c \
          database_common.c \
          localdb_sqlite.c \
          recordset.c \
          log.c \
          parser.c

TEST_SOURCES = protocol.c commands.c conf.c test.c numeric.c
TEST_OBJS = $(TEST_SOURCES:%.c=%.o)

OBJS = $(SOURCES:%.c=%.o)
LIBS = -lsqlite3 -lpthread -lgthread-2.0 -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangocairo-1.0 -lfontconfig -lXext -lXrender -lXinerama -lXi -lXrandr -lXcursor -lXfixes -lpango-1.0 -lcairo -lX11 -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0 -ldbus-1 -ldbus-glib-1

$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(PROGRAM)

test: $(TEST_OBJS)
	$(CC) $(CFLAGS) $(TEST_OBJS) -lm -o test

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c $<

DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(CC) -MM -MG $(SOURCES) > $@

-include $(DEPFILE)

install: $(PROGRAM)
	mkdir -p $(BINDIR)
	cp $(PROGRAM) $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(PROGRAM)


clean:
	rm -f $(SOURCES:%.c=%.o) $(DEPFILE) $(PROGRAM)
