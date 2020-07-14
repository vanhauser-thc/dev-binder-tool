all: servicemanager bindertool

bindertool:	bindertool.c binder.c
	$(CC) -O3 -g -o bindertool -I. bindertool.c binder.c

servicemanager: binder.c service_manager.c
	$(CC) -O3 -g -o $@ -I. $^

clean:
	rm -f *.o servicemanager bindertool core *~

