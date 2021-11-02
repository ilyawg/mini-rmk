CC = gcc
CFLAGS = -O2 -pipe
prefix = /usr/local
exec_prefix = ${prefix}
BINDIR = ${exec_prefix}/bin
DEFS =

PROGRAM = kass_agent
SOURCES = main.c database_common.c sqlite_common.c localdb.c localdb_sqlite.c serverdb_mssql.c process.c conf.c log.c

INCLUDES = -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include
OBJS = $(SOURCES:%.c=%.o)
LIBS = -lsqlite3 -lsybdb -lpthread -ldbus-1

$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $(PROGRAM)

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
