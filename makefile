## wrote by Fanxiushu 2018

vpath %.cpp ./src
vpath %.h   ./src

objs = cJSON.o curl_base.o onedrive.o onedrive_auth.o main.o

cflags = -D_FILE_OFFSET_BITS=64 -D_DARWIN_USE_64_BIT_INODE -D_DARWIN_FEATURE_64_BIT_INODE
links = -lcurl
cl=g++

onedrive-xfsredir:$(objs)
	$(cl) -o ./onedrive-xfsredir $(objs) -lpthread $(links)
%.o:%.cpp
	$(cl) -c $< -o $@  $(cflags)

clean:
	-rm *.o
