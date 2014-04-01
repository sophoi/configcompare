icfdiff: icf.cpp util.cpp icfdiff.cpp path.cpp
	g++ -std=c++11 $^ -o $@
clean:
	rm -f icfdiff
