all: task1

task1: main1.c maind1.c
	gcc -o main1 main1.c -lpthread -lsctp
	gcc -o maind1 maind1.c -lsctp
	
clean:
	rm -f main1 maind1 
