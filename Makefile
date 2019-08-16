cc=g++
bin=IM
src=IM.cc mongoose/mongoose.c
include=-Imongoose -I/usr/include/mysql 
lib=-L/usr/lib64/mysql -lmysqlclient -ljsoncpp
$(bin):$(src)
	$(cc) -o$@ $^ -std=c++11 $(include) $(lib)
.PHONY:clean
clean:
	rm -f $(bin)
