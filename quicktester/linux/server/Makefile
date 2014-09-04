
all: quicktester_server test_meters

quicktester_server: server.c meter.c meter.h log.c log.h
	gcc -g -Wall -std=c99 server.c meter.c log.c -lpthread -o quicktester_server

test_meters: test_meters.c meter.c meter.h log.c log.h
	gcc -g -Wall -std=c99 test_meters.c meter.c log.c -lpthread -o test_meters

clean:
	rm -f quicktester_server test_meters

