demo: clean
	gcc -o demo demo.c pouch.c json.c -lcurl -g
clean:
	-$(RM) demo
