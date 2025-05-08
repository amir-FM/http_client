#include <iostream>
#include <vector>
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

	void add_line(string str){
		message.append(str);
		message.append("\r\n");
	}

	void add_header(string name, string value){
		string aux = name;
		aux += ": ";
		aux += value;

		add_line(aux);
	}

	void add_header(string name, int value){
		string aux = name;
		aux += ": ";
		aux += to_string(value);

		add_line(aux);
	}

	void add_head(string method, string path, string params){
		string aux = method;
		aux += " ";
		aux += path;

		if(params != ""){
			aux += "?";
			aux += params;
		}

		aux += " HTTP/1.1";

		add_line(aux);
	}

	void add_cookies(){
		if(!cookies.size())
			return;
		string aux = "Cookie: ";
		int len = cookies.size();
		for(int i = 0; i < len; i++){
			aux += cookies[i];
			if(i != len - 1)aux += "; ";
		}

		add_line(aux);
	}

	void add_data(string data){
		message += data;
	}

	void compute_get(string path, string host){
		clear_message();
		add_head("GET", path, "");
		add_header("Host", host);
		add_cookies();
		add_line("");
	}

	void compute_post(string path, string host, string type, vector<string> elems){

		clear_message();
		add_head("POST", path, "");
		add_header("Host", host);
		string data = add_urlencoded(elems);
		add_cookies();
		add_line("");
		add_data(data);
	}

	string add_urlencoded(vector<string> elems){
		string data = urlencode(elems);
		add_header("Content-Type", "application/x-www-form-urlencoded");
		add_header("Content-Length", data.size());

		return data;
	}

	string urlencode(vector<string> elems){
		string aux = "";
		int len = elems.size();
		for(int i = 0; i < len; i++){
			aux += elems[i];
			if(i != len - 1)aux += "&";
		}

		return aux;
	}

	string get_message(){
		return message;
	}

	void add_cookie(string cookie){
		cookies.push_back(cookie);
	}

	void clear_cookies(){
		cookies.clear();
	}

private:
	string message;
	vector<string> cookies;
};

class HTTP_Res {
public:
	void add_response(string response){
		this->response = response;
	}

	vector<string> get_cookies(){
		vector<string> cookies;
		int index = 0;
		int rc  = response.find("Set-Cookie", 0);
		while(rc != string::npos){
			cookies.push_back(parse_cookie(response.substr(rc, MAXLEN)));
			rc  = response.find("Set-Cookie", rc + 1);
		}

		return cookies;
	}

	string parse_cookie(string cookie_line){
		int end = cookie_line.find(";");
		string cookie = cookie_line.substr(12, end - 12 + 1);
		cookie.pop_back();
		return cookie;
	}
private:
	string response;
};

int main(){
	TCP t("54.217.160.10", 8080);
	t.start();
	//t.send_message("salut");
	//cout << t.recv_message() << endl;
	//t.stop();
	HTTP h;
	HTTP_Res hres;
	//h.compute_get("/api/v1/dummy", "www.google.com");
	//t.send_message(h.get_message());
	//cout << t.recv_message() << endl;

	vector<string> elems = {"username=student", "password=student"};
	h.compute_post("/api/v1/auth/login", "urlencoded", "54.217.160.10", elems);
	t.send_message(h.get_message());
	hres.add_response(t.recv_message());
	vector<string> cookies = hres.get_cookies();
	for(auto it : cookies)
		cout << it << endl;

	t.stop();
}
