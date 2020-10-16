all: $(patsubst %.cpp, %, $(wildcard *.cpp))

%.out: %.cpp Makefile
	g++ $< -o $@ -std=c++0x

clean:
	find . -type f ! -name .git ! -iname "*.cpp" ! -iname "Makefile" ! -iname "README" ! -iname "test.txt" -delete
