build:
	mkdir -p ./build
	cc blobby_volley.c `pkg-config --libs --cflags raylib` -o ./build/divolley

clean:
	rm -rf ./build

run: clean build
	./build/divolley
