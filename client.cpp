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
		this->tcpfd = -1;
		this->server_ip = server_ip;
		this->port = port;
		fill_addr_in();
	}

	int start(){
		int rc;

		tcpfd = socket(AF_INET, SOCK_STREAM, 0);
		DIE(tcpfd < 0, "socket");

		rc = reconnect();
		if(rc < 0)return -1;

		return 0;
	}

	int fill_addr_in(){
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		int rc = inet_pton(AF_INET, server_ip.data(), &addr.sin_addr.s_addr);
		if(rc <= 0){
			ERR("invalid IP address");
			stop();
			return -1;
		}
		return 0;
	}

	int reconnect(){
		int rc = connect(tcpfd, (struct sockaddr *)&addr, sizeof(addr));
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
		int rc = start();
		if(rc < 0)return -1;
		return send_all(message);
	}
	
	string recv_message(){
		string message = "";
		int rc = recv_all(message);
		if(rc < 0)
			return "";
		stop();
		return message;
	}

private:
	string server_ip;
	uint16_t port;
	int tcpfd;
	bool is_connected;
	struct sockaddr_in addr;

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
			if(rc < 0){
				ERR("could not send message");
				return -1;
			}

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
		int rc;
		while(1){
			int rc = recv(tcpfd, buf, MAXLEN, 0);
			if(rc < 0){
				ERR("could not send message");
				return -1;
			}

			if(!rc)break;
			message.append(buf, 0, rc);
		}

		return 0;
	}

};

class HTTP {
public:
	HTTP(){}
	HTTP(string host){
		this->host = host;
	}
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

	void compute_get(string path){
		clear_message();
		add_head("GET", path, "");
		add_header("Host", host);
		add_cookies();
		add_line("");
	}

	void compute_post(string path, string type, vector<string> elems){

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

	void add_cookie(vector<string> to_add){
		this->cookies.insert(this->cookies.end(), to_add.begin(), to_add.end());
	}

	void clear_cookies(){
		cookies.clear();
	}

private:
	string message;
	string host;
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
	TCP t("63.32.125.183", 8081);
	HTTP h("63.32.125.183");
	HTTP_Res hres;

	h.compute_get("/api/v1/tema/admin/users");
	cout << h.get_message() << endl;
	t.send_message(h.get_message());
	cout << t.recv_message() << endl;

	cout << h.get_message() << endl;
	t.send_message(h.get_message());
	cout << t.recv_message() << endl;

}
