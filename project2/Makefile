all: np_simple np_single_proc np_multi_proc clean

np_simple: np_simple.o npshell1.o connectsock.o connectTCP.o connectUDP.o errexit.o passivesock.o passiveTCP.o passiveUDP.o proj2bi.o
	@g++ -o np_simple np_simple.o npshell1.o connectsock.o connectTCP.o connectUDP.o errexit.o passivesock.o passiveTCP.o passiveUDP.o proj2bi.o

np_simple.o: np_simple.cpp npshell1.cpp 
	@g++ -c np_simple.cpp npshell1.cpp ./srcmd/*.cpp

np_single_proc: np_single_proc.o npshell2.o connectsock.o connectTCP.o connectUDP.o errexit.o passivesock.o passiveTCP.o passiveUDP.o proj2bi.o
	@g++ -o np_single_proc np_single_proc.o npshell2.o connectsock.o connectTCP.o connectUDP.o errexit.o passivesock.o passiveTCP.o passiveUDP.o proj2bi.o

np_single_proc.o: np_single_proc.cpp npshell2.cpp
	@g++ -c np_single_proc.cpp npshell2.cpp ./srcmd/*.cpp

np_multi_proc: np_multi_proc.o connectsock.o connectTCP.o connectUDP.o errexit.o passivesock.o passiveTCP.o passiveUDP.o proj2bi.o
	@g++ -o np_multi_proc np_multi_proc.o connectsock.o connectTCP.o connectUDP.o errexit.o passivesock.o passiveTCP.o passiveUDP.o proj2bi.o -pthread

np_multi_proc.o: np_multi_proc.cpp
	@g++ -c np_multi_proc.cpp ./srcmd/*.cpp

clean:
	@rm *.o