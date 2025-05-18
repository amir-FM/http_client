client: client.cpp
	g++ client.cpp -o client

clean:
	- rm client *.zip

pack:
	zip 323CC_Fallah-Mirzaei_Amir_Tema3PC client.cpp helper.h json.hpp Makefile README.md
