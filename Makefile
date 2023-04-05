hw2: testcase.o hw1.o hw2.o disk.o
	gcc -o hw2 testcase.o hw1.o hw2.o disk.o

testcase.o: testcase.c
	gcc -c -o testcase.o testcase.c

hw1.o: hw1.c
	gcc -c -o hw1.o hw1.c

hw2.o: hw2.c
	gcc -c -o hw2.o hw2.c

disk.o: disk.c
	gcc -c -o disk.o disk.c

clean:
	rm hw2 testcase.o hw1.o hw2.o disk.o