CC = gcc
CFLAGS = -O2 -pipe
prefix = /usr/local
exec_prefix = ${prefix}
BINDIR = ${exec_prefix}/bin
DEFS =

PROGRAM = kass_check
SOURCES = main.c \
          database_common.c \
          localdb.c \
          localdb_sqlite.c \
          sqlite_common.c \
          serverdb.c \
          serverdb_mssql.c \
          process.c \
          conf.c \
          log.c

OBJS = $(SOURCES:%.c=%.o)
LIBS = -lsqlite3 -lsybdb

$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(PROGRAM)

%.o: %.c
	$(CC) $(CFLAGS) $(DEFS) -c $<

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
