#include <iostream>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "helper.h"

using namespace std;

class TCP {
public:
	TCP(){
		this->tcpfd = -1;
	}

	TCP(string server_ip, uint16_t port){
		this->server_ip = server_ip;
		this->port = port;
	}

	int start(){
		int rc;

		tcpfd = socket(AF_INET, SOCK_STREAM, 0);
		DIE(tcpfd < 0, "socket");

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		rc = inet_pton(AF_INET, server_ip.data(), &addr.sin_addr.s_addr);
		if(rc <= 0){
			ERR("invalid IP address");
			stop();
			return -1;
		}

		rc = connect(tcpfd, (struct sockaddr *)&addr, sizeof(addr));
		if(rc < 0){
			ERR("could not connect to server");
			stop();
			return -1;
		}

		return 0;
	}

	void stop(){
		close(tcpfd);
		tcpfd = -1;
	}

	int send_message(string message){
		return send_all(message);
	}
	
	string recv_message(){
		string message = "";
		int rc = recv_all(message);
		if(rc < 0)
			return "";
		return message;
	}

private:
	string server_ip;
	uint16_t port;
	int tcpfd;

	int send_all(string message){
		if(tcpfd == -1){
			ERR("TCP not started");
			return -1;
		}

		char *buf = message.data();
		int rc;
		int len = message.size();
		int sent = 0;
		while(sent != len){
			int rc = send(tcpfd, buf + sent, len - sent, 0);
			if(rc < 0)
				ERR("could not send message");

			if(!rc)break;
			sent += rc;
		}

		return 0;
	}

	int recv_all(string &message){
		if(tcpfd == -1){
			ERR("TCP not started");
			return -1;
		}

		char buf[MAXLEN];
		while(1){
			int rc = recv(tcpfd, buf, MAXLEN, 0);
			if(rc < 0)
				ERR("could not recv message");
			if(!rc)break;
			
			string aux = buf;
			message.append(aux, 0, rc);
		}

		return 0;
	}

};

class HTTP {
public:
	void clear_message(){
		this->message = "";
	}
private:
	string message;
}

int main(){
	TCP t("127.0.0.1", 8080);
	t.start();
	t.send_message("salut");
	cout << t.recv_message() << endl;
	t.stop();

}
