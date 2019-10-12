# ISA 2014/2015
# IRC bot s logováním přes SYSLOG protokol
#
# Jan Havran <xhavra13@stud.fit.vutbr.cz>

CC		= gcc
CFLAGS		= -Wall -Werror
LDFLAGS		= -lpthread
IRC_SERVER	= irc.freenode.net
IRC_CHANNEL	= testfit
SYSLOG_SERVER	= 127.0.0.1
MATCH_WORDS	= "fit vutbr;lan;wan"
EXEC		= ircbot

all: $(EXEC)

$(EXEC): isa_main.o isa_irc.o isa_syslog.o isa_queue.o
	$(CC) isa_main.o isa_irc.o isa_syslog.o isa_queue.o -o $(EXEC) $(LDFLAGS)
isa_main.o: isa_main.c
	$(CC) isa_main.c -c $(CFLAGS)
isa_irc.o: isa_irc.c
	$(CC) isa_irc.c -c $(CFLAGS)
isa_syslog.o: isa_syslog.c
	$(CC) isa_syslog.c -c $(CFLAGS)
isa_queue.o: isa_queue.c
	$(CC) isa_queue.c -c $(CFLAGS)
run: $(EXEC)
	./$(EXEC) $(IRC_SERVER) "#"$(IRC_CHANNEL) $(SYSLOG_SERVER) $(MATCH_WORDS)
pack:
	tar -cvf xhavra13.tar ./*.c ./*.h Makefile README manual.pdf
clean:
	rm -f *.o $(EXEC)
