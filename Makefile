all: travelMonitorClient monitorServer

IDIR =../project1
CC= g++ -g
CFLAGS=-I$(IDIR) -std=c++11 -g #-Wall

ODIR = project1

OBJ_DIR = $(patsubst %,$(ODIR)/%,$(_OBJ))

_OBJ = Date.o Citizen.o Record.o HashTable.o SkipList.o BloomFilter.o Application.o AdditionFunctions.o

OBJS1 = monitorServer.o Send_Get.o CyclicBuffer.o

OBJS2 = travelMonitorClient.o TravelRequests.o Send_Get.o

EXEC1 = monitorServer
EXEC2 = travelMonitorClient

$(EXEC1): $(OBJS1) $(OBJ_DIR) 
	$(CC) $(CFLAGS) $(OBJ_DIR) $(OBJS1) -o $(EXEC1) -lpthread

$(EXEC2): $(OBJS2) $(OBJ_DIR)
	$(CC) $(CFLAGS)  $(OBJ_DIR) $(OBJS2) -o $(EXEC2) 


clean:
	rm -f $(EXEC1) logfiles/* $(EXEC2) $(OBJS1) $(OBJS2) $(ODIR)/*.o *~ core $(IDIR)/*~ 

