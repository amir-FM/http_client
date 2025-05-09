#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "helper.h"
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

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
	
	void recv_message(){
		this->recvd_message = "";
		int rc = recv_all(this->recvd_message);
		stop();
	}

	string get_response(){
		return this->recvd_message;
	}

private:
	string server_ip;
	uint16_t port;
	int tcpfd;
	bool is_connected;
	struct sockaddr_in addr;
	string recvd_message;

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
		this->jwt = "";
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

	void compute_get_jwt(string path){
		clear_message();
		add_head("GET", path, "");
		add_header("Host", host);
		add_header("Authorization", jwt);
		add_cookies();
		add_line("");
	}

	void compute_get_jwt(string path, string elem){
		clear_message();
		add_head("GET", path + "/" + elem, "");
		add_header("Host", host);
		add_header("Authorization", jwt);
		add_cookies();
		add_line("");
	}

	void compute_delete(string path, string elem){
		clear_message();
		add_head("DELETE", path + "/" + elem, "");
		add_header("Host", host);
		add_cookies();
		add_line("");
	}

	void compute_post(string path, vector<string> elems){
		clear_message();
		add_head("POST", path, "");
		add_header("Host", host);
		string data = add_urlencoded(elems);
		add_cookies();
		add_line("");
		add_data(data);
	}

	void compute_post_jwt(string path, json j){
		clear_message();
		add_head("POST", path, "");
		add_header("Host", host);
		add_header("Authorization", jwt);
		string data = add_json(j);
		add_cookies();
		add_line("");
		add_data(data);
	}

	void compute_post(string path, json j){
		clear_message();
		add_head("POST", path, "");
		add_header("Host", host);
		string data = add_json(j);
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

	string add_json(json j){
		string data = j.dump();
		add_header("Content-Type", "application/json");
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

	void add_jwt(string jwt){
		this->jwt = "Bearer " + jwt;
	}

	void clear_cookies(){
		cookies.clear();
		this->jwt = "";
	}

private:
	string message;
	string host;
	vector<string> cookies;
	string jwt;
};

class HTTP_Res {
public:
	void add_response(string response){
		this->response = response;
		this->data = "";
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

	string get_data(){
		int start = response.find("\r\n\r\n", 0);
		if(start == string::npos)
			return "";

		start += 4;
		this->data = response.substr(start, response.size() - start);
		return this->data;
	}

	void parse_json(){
		try{
			parsed_data = json::parse(this->data);
		}catch(...){
			parsed_data = json({});
		}
	}

	string get_error(){
		parse_json();
		if(parsed_data.empty())
			return "bad recv";
		return parsed_data["error"];
	}

	json get_json(){
		parse_json();
		return parsed_data;
	}

	bool check_response(){
		string first_line = response.substr(0, response.find("\r", 0));

		if(first_line.find("20", 0) != string::npos)
			return true;

		return false;
	}

private:
	string response;
	string data;
	json parsed_data;
};

class Client {
public:
	Client (){}
	Client (string ip, uint16_t port){
		this->t = new TCP(ip, port);
		this->h = new HTTP(ip);
		this->hres = new HTTP_Res();
	}

	string get_input(string str){
		cout << str << "=";
		string in;
		cin >> in;
		return in;
	}

	string get_input_line(string str){
		cout << str << "=";
		string in;
		getline(cin, in);
		while(in.empty())
			getline(cin, in);
		return in;
	}

	void success(string str){
		cout << "SUCCESS: " << str << endl;
	}

	void error(string str){
		cout << "ERROR: " << str << endl;
	}

	void login_admin(){
		string user = get_input("username");
		string pass = get_input("password");
		json j;
		j["username"] = user;
		j["password"] = pass;
		h->compute_post("/api/v1/tema/admin/login", j);
		cout << h->get_message() << endl;
		put_message();
		if(hres->check_response()){
			h->add_cookie(hres->get_cookies());
			success("Admin autentificat cu succes");
		}else{
			error(hres->get_error());
		}
	}

	void add_user(){
		string user = get_input("username");
		string pass = get_input("password");
		json j;
		j["username"] = user;
		j["password"] = pass;
		h->compute_post("/api/v1/tema/admin/users", j);
		cout << h->get_message() << endl;
		put_message();
		if(hres->check_response()){
			success("Utilizator adaugat cu succes");
		}else{
			error(hres->get_error());
		}
	}

	void get_users(){
		h->compute_get("/api/v1/tema/admin/users");
		cout << h->get_message() << endl;
		put_message();
		if(hres->check_response()){
			json j = hres->get_json();
			success("Lista utilizatorilor");
			int index = 1;
			for(auto it : j["users"])
				cout << "#" << index++ << " " << it["username"].dump() << ":" << it["password"].dump() << endl;
		}else{
			error(hres->get_error());
		}
	}

	void delete_user(){
		string user = get_input("username");
		h->compute_delete("/api/v1/tema/admin/users", user);
		cout << h->get_message() << endl;
		put_message();
		if(hres->check_response()){
			success("Utilizator sters");
		}else{
			error(hres->get_error());
		}
	}

	void logout_admin(){
		h->compute_get("/api/v1/tema/admin/logout");
		cout << h->get_message() << endl;
		put_message();
		if(hres->check_response()){
			h->clear_cookies();
			success("Admin delogat");
		}else{
			error(hres->get_error());
		}
	}

	void login(){
		string admin = get_input("admin_username");
		string user = get_input("username");
		string pass = get_input("password");
		json j;
		j["admin_username"] = admin;
		j["username"] = user;
		j["password"] = pass;
		h->compute_post("/api/v1/tema/user/login", j);
		cout << h->get_message() << endl;
		put_message();
		if(hres->check_response()){
			h->add_cookie(hres->get_cookies());
			success("Autentificare reusita");
		}else{
			error(hres->get_error());
		}
	}

	void get_access(){
		h->compute_get("/api/v1/tema/library/access");
		cout << h->get_message() << endl;
		put_message();
		cout << t->get_response() << endl;
		if(hres->check_response()){
			string jwt = hres->get_json()["token"];
			h->add_jwt(jwt);
			success("Token JWT primit");
		}else{
			error(hres->get_error());
		}
	}

	void get_movies(){
		h->compute_get_jwt("/api/v1/tema/library/movies");
		cout << h->get_message() << endl;
		put_message();
		cout << t->get_response() << endl;
		if(hres->check_response()){
			json data = hres->get_json();
			success("Lista filmelor");
			for(auto it : data["movies"]){
				cout << "#" << it["id"] << " " << it["title"] << endl;
			}
		}else{
			error(hres->get_error());
		}
	}

	void get_movie(){
		string id = get_input("id");
		h->compute_get_jwt("/api/v1/tema/library/movies", id);
		cout << h->get_message() << endl;
		put_message();
		cout << t->get_response() << endl;
		if(hres->check_response()){
			json data = hres->get_json();
			success("Detalii film");
			cout << "id: " << data["id"] << endl;
			cout << "title: " << data["title"] << endl;
			cout << "year: " << data["year"] << endl;
			cout << "description: " << data["description"] << endl;
			cout << "rating: " << data["rating"] << endl;
		}else{
			error(hres->get_error());
		}
	}

	void add_movie(){
		string title = get_input_line("title");
		string year = get_input("year");
		string desc = get_input_line("description");
		string rating = get_input("rating");

		json j;
		j["title"] = title;
		j["year"] = year;
		j["description"] = desc;
		j["rating"] = rating;
		h->compute_post_jwt("/api/v1/tema/library/movies", j);
		cout << h->get_message() << endl;
		put_message();
		if(hres->check_response()){
			success("Film adaugat");
		}else{
			error(hres->get_error());
		}
	}

	void run(){
		while(1){
			string choice;
			cin >> choice;
			if(choice == "login_admin")
				login_admin();
			else if(choice == "add_user")
				add_user();
			else if(choice == "get_users")
				get_users();
			else if(choice == "delete_user")
				delete_user();
			else if(choice == "logout_admin")
				logout_admin();
			else if(choice == "login")
				login();
			else if(choice == "get_access")
				get_access();
			else if(choice == "get_movies")
				get_movies();
			else if(choice == "get_movie")
				get_movie();
			else if(choice == "add_movie")
				add_movie();
			else if(choice == "exit")
				return;
		}
	}

private:
	void put_message(){
		t->send_message(h->get_message());
		t->recv_message();
		hres->add_response(t->get_response());
		hres->get_data();
	}

	TCP *t;
	HTTP *h;
	HTTP_Res *hres;
};

int main(){
	Client c("63.32.125.183", 8081);
	c.run();
}
