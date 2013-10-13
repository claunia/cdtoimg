OBJS = FloatUtils.o cdtoimg.o
CC = g++
DEBUG = -g
CFLAGS = -Wall -O0 -W -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)
LIBS = -lm -lcdio

cdtoimg : $(OBJS)
	$(CC) $(LFLAGS) $(LIBS) $(OBJS) -o cdtoimg

FloatUtils.o : FloatUtils.h FloatUtils.cpp
	$(CC) $(CFLAGS) FloatUtils.cpp

cdtoimg.o : cdtoimg.cpp FloatUtils.h
	$(CC) $(CFLAGS) cdtoimg.cpp

clean:
	\rm *.o cdtoimg

tar: $(OBJS)
	tar jcfv cdtoimg.tbz FloatUtils.h FloatUtils.cpp readme.txt Makefile cdtoimg
