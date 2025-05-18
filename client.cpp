#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "helper.h"
#include "json.hpp"

#define DEBUG 0
#define MAXLEN 1000

using namespace std;
using json = nlohmann::json;

class TCP {
 public:
  TCP() { this->tcpfd = -1; }

  TCP(string server_ip, uint16_t port) {
    this->tcpfd = -1;
    this->server_ip = server_ip;
    this->port = port;
    fill_addr_in();
  }

  int start() {
    int rc;

    tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcpfd < 0, "socket");

    rc = connect(tcpfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
      ERR("could not connect to server");
      stop();
      return -1;
    }

    return 0;
  }

  void stop() {
    close(tcpfd);
    tcpfd = -1;
  }

  int send_message(string message) {
    return send_all(message);
  }

  int recv_message() {
    this->recvd_message = "";
    int rc = recv_all(this->recvd_message);
    return rc;
  }

  string get_response() { return this->recvd_message; }

  void restart() {
    stop();
    start();
  }

 private:
  string server_ip;
  uint16_t port;
  int tcpfd;
  bool is_connected;
  struct sockaddr_in addr;
  string recvd_message;

  int send_all(string message) {
    if (tcpfd == -1) {
      ERR("TCP not started");
      return -1;
    }

    char *buf = message.data();
    int rc;
    int len = message.size();
    int sent = 0;
    while (sent != len) {
      int rc = send(tcpfd, buf + sent, len - sent, 0);
      if (rc < 0) {
        ERR("could not send message");
        return -1;
      }

      if (!rc) break;
      sent += rc;
    }

    return 0;
  }

  int recv_all(string &message) {
    if (tcpfd == -1) {
      ERR("TCP not started");
      return -1;
    }

    int body_len = -1;
    int end = -1;
    char buf[MAXLEN];
    while (1) {
      int rc = recv(tcpfd, buf, MAXLEN, 0);
      if (rc <= 0) {
        ERR("could not recv message");
        return -1;
      }

      if (body_len == -1) body_len = search_body_len(buf, rc);
      message.append(buf, 0, rc);
      end = search_for_end(buf, rc);
      if (end != -1) break;
    }

    if (body_len == -1) return 0;

    body_len -= end;
    while (body_len) {
      int rc = recv(tcpfd, buf, MAXLEN, 0);
      if (rc <= 0) {
        ERR("could not recv message");
        return -1;
      }

      body_len -= rc;
      message.append(buf, 0, rc);
    }

    return 0;
  }

  int search_body_len(char *buf, int size) {
    string str = buf;
    str = str.substr(0, size);
    int start = str.find("Content-Length", 0);
    if (start == string::npos) return -1;

    int end = str.find("\r", start);
    if (end == string::npos) return -1;
    string num = str.substr(start + 16, end - start - 16);
    int fnum;
    try {
      fnum = stoi(num);
    } catch (...) {
      ERR("error at string to num conversion - search_body_len");
      return -1;
    }

    return fnum;
  }

  int search_for_end(char *buf, int size) {
    string str = buf;
    str = str.substr(0, size);
    int pos = str.find("\r\n\r\n", 0);
    if (pos == string::npos) return -1;

    return str.size() - pos - 4;
  }

  int fill_addr_in() {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    int rc = inet_pton(AF_INET, server_ip.data(), &addr.sin_addr.s_addr);
    if (rc <= 0) {
      ERR("invalid IP address");
      stop();
      return -1;
    }
    return 0;
  }
};

class HTTP {
 public:
  HTTP() {}
  HTTP(string host) {
    this->host = host;
    this->jwt = "";
  }

  void compute_get(string path) {
    clear_message();
    add_head("GET", path, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    add_cookies();
    add_line("");
  }

  void compute_get_jwt(string path) {
    clear_message();
    add_head("GET", path, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    add_header("Authorization", jwt);
    add_cookies();
    add_line("");
  }

  void compute_get_jwt(string path, string elem) {
    clear_message();
    add_head("GET", path + "/" + elem, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    add_header("Authorization", jwt);
    add_cookies();
    add_line("");
  }

  void compute_delete(string path, string elem) {
    clear_message();
    add_head("DELETE", path + "/" + elem, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    add_cookies();
    add_line("");
  }

  void compute_delete_jwt(string path, string elem) {
    clear_message();
    add_head("DELETE", path + "/" + elem, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    add_header("Authorization", jwt);
    add_cookies();
    add_line("");
  }

  void compute_post(string path, vector<string> elems) {
    clear_message();
    add_head("POST", path, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    string data = add_urlencoded(elems);
    add_cookies();
    add_line("");
    add_data(data);
  }

  void compute_post_jwt(string path, json j) {
    clear_message();
    add_head("POST", path, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    add_header("Authorization", jwt);
    string data = add_json(j);
    add_cookies();
    add_line("");
    add_data(data);
  }

  void compute_put_jwt(string path, string elem, json j) {
    clear_message();
    add_head("PUT", path + "/" + elem, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    add_header("Authorization", jwt);
    string data = add_json(j);
    add_cookies();
    add_line("");
    add_data(data);
  }

  void compute_post(string path, json j) {
    clear_message();
    add_head("POST", path, "");
    add_header("Host", host);
    add_header("Connection", "keep-alive");
    string data = add_json(j);
    add_cookies();
    add_line("");
    add_data(data);
  }

  string get_message() { return message; }

  void add_cookie(string cookie) { cookies.push_back(cookie); }

  void add_cookie(vector<string> to_add) {
    this->cookies.insert(this->cookies.end(), to_add.begin(), to_add.end());
  }

  void add_jwt(string jwt) { this->jwt = "Bearer " + jwt; }

  void clear_cookies() {
    cookies.clear();
    this->jwt = "";
  }

 private:
  string message;
  string host;
  vector<string> cookies;
  string jwt;

  void clear_message() { this->message = ""; }

  void add_line(string str) {
    message.append(str);
    message.append("\r\n");
  }

  void add_header(string name, string value) {
    string aux = name;
    aux += ": ";
    aux += value;

    add_line(aux);
  }

  void add_header(string name, int value) {
    string aux = name;
    aux += ": ";
    aux += to_string(value);

    add_line(aux);
  }

  void add_head(string method, string path, string params) {
    string aux = method;
    aux += " ";
    aux += path;

    if (params != "") {
      aux += "?";
      aux += params;
    }

    aux += " HTTP/1.1";

    add_line(aux);
  }

  void add_cookies() {
    if (!cookies.size()) return;
    string aux = "Cookie: ";
    int len = cookies.size();
    for (int i = 0; i < len; i++) {
      aux += cookies[i];
      if (i != len - 1) aux += "; ";
    }

    add_line(aux);
  }

  void add_data(string data) { message += data; }

  string add_urlencoded(vector<string> elems) {
    string data = urlencode(elems);
    add_header("Content-Type", "application/x-www-form-urlencoded");
    add_header("Content-Length", data.size());

    return data;
  }

  string add_json(json j) {
    string data = j.dump();
    add_header("Content-Type", "application/json");
    add_header("Content-Length", data.size());

    return data;
  }

  string urlencode(vector<string> elems) {
    string aux = "";
    int len = elems.size();
    for (int i = 0; i < len; i++) {
      aux += elems[i];
      if (i != len - 1) aux += "&";
    }

    return aux;
  }
};

class HTTP_Res {
 public:
  void add_response(string response) {
    this->response = response;
    this->data = "";
  }

  vector<string> get_cookies() {
    vector<string> cookies;
    int index = 0;
    int rc = response.find("Set-Cookie", 0);
    while (rc != string::npos) {
      cookies.push_back(parse_cookie(response.substr(rc, MAXLEN)));
      rc = response.find("Set-Cookie", rc + 1);
    }

    return cookies;
  }

  string get_data() {
    int start = response.find("\r\n\r\n", 0);
    if (start == string::npos) return "";

    start += 4;
    this->data = response.substr(start, response.size() - start);
    return this->data;
  }

  string get_error() {
    parse_json();
    if (parsed_data.empty()) {
      return "bad recv";
    }
    return parsed_data["error"];
  }

  json get_json() {
    parse_json();
    return parsed_data;
  }

  bool check_response() {
    string first_line = response.substr(0, response.find("\r", 0));

    if (first_line.find("20", 0) != string::npos) return true;

    return false;
  }

 private:
  string response;
  string data;
  json parsed_data;

  string parse_cookie(string cookie_line) {
    int end = cookie_line.find(";");
    string cookie = cookie_line.substr(12, end - 12 + 1);
    cookie.pop_back();
    return cookie;
  }

  void parse_json() {
    try {
      parsed_data = json::parse(this->data);
    } catch (...) {
      parsed_data = json({});
    }
  }
};

class Client {
 public:
  Client() {}
  Client(string ip, uint16_t port) {
    this->t = new TCP(ip, port);
    this->h = new HTTP(ip);
    this->hres = new HTTP_Res();
    t->start();
  }

  void run() {
    while (1) {
      string choice;
      cin >> choice;
      if (choice == "login_admin")
        login_admin();
      else if (choice == "add_user")
        add_user();
      else if (choice == "get_users")
        get_users();
      else if (choice == "delete_user")
        delete_user();
      else if (choice == "logout_admin")
        logout_admin();
      else if (choice == "login")
        login();
      else if (choice == "get_access")
        get_access();
      else if (choice == "get_movies")
        get_movies();
      else if (choice == "get_movie")
        get_movie();
      else if (choice == "add_movie")
        add_movie();
      else if (choice == "update_movie")
        update_movie();
      else if (choice == "delete_movie")
        delete_movie();
      else if (choice == "get_collections")
        get_collections();
      else if (choice == "get_collection")
        get_collection();
      else if (choice == "add_collection")
        add_collection_routine();
      else if (choice == "delete_collection")
        delete_collection();
      else if (choice == "add_movie_to_collection")
        add_movie_to_collection();
      else if (choice == "delete_movie_from_collection")
        delete_movie_from_collection();
      else if (choice == "logout")
        logout();
      else if (choice == "exit") {
        t->stop();
        return;
      } else {
        error("comanda invalida");
      }
    }
  }

 private:
  TCP *t;
  HTTP *h;
  HTTP_Res *hres;

  void put_message() {
    int max_tries = 5;
    while(max_tries--){
      int rc = t->send_message(h->get_message());
      if(rc < 0){
        t->restart();
        continue;
      }
      rc = t->recv_message();
      if(rc < 0){
        t->restart();
        continue;
      }
      break;
    }
    hres->add_response(t->get_response());
    hres->get_data();
  }

  bool check_no_spaces(string input) {
    if (input.find(" ", 0) != string::npos) {
      error("Datele nu au tipul cerut");
      return true;
    }
    return false;
  }

  string get_input(string str) {
    cout << str << "=";
    string in;
    cin >> in;
    return in;
  }

  string get_input_line(string str) {
    cout << str << "=";
    string in;
    getline(cin, in);
    while (in.empty()) getline(cin, in);
    return in;
  }

  void success(string str) { cout << "SUCCESS: " << str << endl; }

  void error(string str) { cout << "ERROR: " << str << endl; }

  void login_admin() {
    string user = get_input_line("username");
    string pass = get_input_line("password");
    if (check_no_spaces(user)) return;
    if (check_no_spaces(pass)) return;
    json j;
    j["username"] = user;
    j["password"] = pass;
    h->compute_post("/api/v1/tema/admin/login", j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      h->add_cookie(hres->get_cookies());
      success("Admin autentificat cu succes");
    } else {
      error(hres->get_error());
    }
  }

  void add_user() {
    string user = get_input_line("username");
    string pass = get_input_line("password");
    if (check_no_spaces(user)) return;
    if (check_no_spaces(pass)) return;
    json j;
    j["username"] = user;
    j["password"] = pass;
    h->compute_post("/api/v1/tema/admin/users", j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("utilizator adaugat cu succes");
    } else {
      error(hres->get_error());
    }
  }

  void get_users() {
    h->compute_get("/api/v1/tema/admin/users");
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      json j = hres->get_json();
      success("Lista utilizatorilor");
      for (auto it : j["users"])
        cout << "#" << it["id"] << " " << it["username"].get<string>() << ":"
             << it["password"].get<string>() << endl;
    } else {
      error(hres->get_error());
    }
  }

  void delete_user() {
    string user = get_input_line("username");
    if (check_no_spaces(user)) return;
    h->compute_delete("/api/v1/tema/admin/users", user);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Utilizator sters");
    } else {
      error(hres->get_error());
    }
  }

  void logout_admin() {
    h->compute_get("/api/v1/tema/admin/logout");
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      h->clear_cookies();
      success("Admin delogat");
    } else {
      error(hres->get_error());
    }
  }

  void login() {
    string admin = get_input_line("admin_username");
    string user = get_input_line("username");
    string pass = get_input_line("password");
    if (check_no_spaces(admin)) return;
    if (check_no_spaces(user)) return;
    if (check_no_spaces(pass)) return;
    json j;
    j["admin_username"] = admin;
    j["username"] = user;
    j["password"] = pass;
    h->compute_post("/api/v1/tema/user/login", j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      h->add_cookie(hres->get_cookies());
      success("Autentificare reusita");
    } else {
      error(hres->get_error());
    }
  }

  void get_access() {
    h->compute_get("/api/v1/tema/library/access");
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (DEBUG) cout << t->get_response() << endl;
    if (hres->check_response()) {
      string jwt = hres->get_json()["token"];
      h->add_jwt(jwt);
      success("Token JWT primit");
    } else {
      error(hres->get_error());
    }
  }

  void get_movies() {
    h->compute_get_jwt("/api/v1/tema/library/movies");
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (DEBUG) cout << t->get_response() << endl;
    if (hres->check_response()) {
      json data = hres->get_json();
      success("Lista filmelor");
      for (auto it : data["movies"]) {
        cout << "#" << it["id"] << " " << it["title"].get<string>() << endl;
      }
    } else {
      error(hres->get_error());
    }
  }

  void get_movie() {
    string id = get_input("id");
    try {
      stoi(id);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_get_jwt("/api/v1/tema/library/movies", id);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (DEBUG) cout << t->get_response() << endl;
    if (hres->check_response()) {
      json data = hres->get_json();
      success("Detalii film");
      cout << "id: " << data["id"] << endl;
      cout << "title: " << data["title"].get<string>() << endl;
      cout << "year: " << data["year"] << endl;
      cout << "description: " << data["description"].get<string>() << endl;
      try {
        cout << "rating: " << stof(data["rating"].get<string>()) << endl;
      } catch (...) {
        cout << "rating: " << data["rating"] << endl;
      }
    } else {
      error(hres->get_error());
    }
  }

  void add_movie() {
    string title = get_input_line("title");
    string year = get_input("year");
    string desc = get_input_line("description");
    string rating = get_input("rating");

    json j;
    try {
      j["title"] = title;
      j["year"] = stoi(year);
      j["description"] = desc;
      j["rating"] = stof(rating);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_post_jwt("/api/v1/tema/library/movies", j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Film adaugat");
    } else {
      error(hres->get_error());
    }
  }

  void update_movie() {
    string id = get_input("id");
    string title = get_input_line("title");
    string year = get_input("year");
    string desc = get_input_line("description");
    string rating = get_input("rating");

    json j;
    try {
      j["title"] = title;
      j["year"] = stoi(year);
      j["description"] = desc;
      j["rating"] = stof(rating);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_put_jwt("/api/v1/tema/library/movies", id, j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Film actualizat");
    } else {
      error(hres->get_error());
    }
  }

  void delete_movie() {
    string id = get_input("id");
    try {
      stoi(id);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_delete_jwt("/api/v1/tema/library/movies", id);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Film sters");
    } else {
      error(hres->get_error());
    }
  }

  int get_collections() {
    h->compute_get_jwt("/api/v1/tema/library/collections");
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (DEBUG) cout << t->get_response() << endl;
    if (hres->check_response()) {
      json data = hres->get_json();
      success("Lista colectiilor");
      for (auto it : data["collections"]) {
        cout << "#" << it["id"] << " " << it["title"].get<string>() << endl;
      }
      return 0;
    } else {
      error(hres->get_error());
      return -1;
    }
  }

  int get_collections(json &res) {
    h->compute_get_jwt("/api/v1/tema/library/collections");
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (DEBUG) cout << t->get_response() << endl;
    if (hres->check_response()) {
      json data = hres->get_json();
      res = data;
      return 0;
    } else {
      return -1;
    }
  }

  void get_collection() {
    string id = get_input("id");
    try {
      stoi(id);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_get_jwt("/api/v1/tema/library/collections", id);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (DEBUG) cout << t->get_response() << endl;
    if (hres->check_response()) {
      json data = hres->get_json();
      success("Detalii colectie");
      cout << "id: " << data["id"] << endl;
      cout << "title: " << data["title"].get<string>() << endl;
      cout << "owner: " << data["owner"].get<string>() << endl;
      for (auto it : data["movies"])
        cout << "#" << it["id"] << " " << it["title"].get<string>() << endl;
    } else {
      error(hres->get_error());
    }
  }

  void get_collection(string id) {
    try {
      stoi(id);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_get_jwt("/api/v1/tema/library/collections", id);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (DEBUG) cout << t->get_response() << endl;
    if (hres->check_response()) {
      json data = hres->get_json();
      success("Detalii colectie");
      cout << "id: " << data["id"] << endl;
      cout << "title: " << data["title"].get<string>() << endl;
      cout << "owner: " << data["owner"].get<string>() << endl;
      for (auto it : data["movies"])
        cout << "#" << it["id"] << " " << it["title"].get<string>() << endl;
    } else {
      error(hres->get_error());
    }
  }

  int add_collection() {
    string title = get_input_line("title");
    json j;
    j["title"] = title;
    h->compute_post_jwt("/api/v1/tema/library/collections", j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Colectie creeata");
      return 0;
    } else {
      error(hres->get_error());
      return -1;
    }
  }

  int add_collection(string title) {
    json j;
    j["title"] = title;
    h->compute_post_jwt("/api/v1/tema/library/collections", j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      return 0;
    } else {
      return -1;
    }
  }

  void delete_collection() {
    string id = get_input("id");
    try {
      stoi(id);
    } catch (...) {
      error("Datele nu au tipul cerut");
    }
    h->compute_delete_jwt("/api/v1/tema/library/collections", id);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Colectie stearsa");
    } else {
      error(hres->get_error());
    }
  }

  void add_movie_to_collection() {
    string collection_id = get_input("collection_id");
    string movie_id = get_input("movie_id");
    json j;
    try {
      stoi(collection_id);
      j["id"] = stoi(movie_id);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_post_jwt(
        "/api/v1/tema/library/collections/" + collection_id + "/movies", j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Film adaugat la colectie");
    } else {
      error(hres->get_error());
    }
  }

  int add_movie_to_collection(string id, string movie_id) {
    json j;
    try {
      stoi(id);
      j["id"] = stoi(movie_id);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return -1;
    }
    h->compute_post_jwt("/api/v1/tema/library/collections/" + id + "/movies",
                        j);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      return 0;
    } else {
      return -1;
    }
  }

  void delete_movie_from_collection() {
    string collection_id = get_input("collection_id");
    string movie_id = get_input("movie_id");
    try {
      stoi(collection_id);
      stoi(movie_id);
    } catch (...) {
      error("Datele nu au tipul cerut");
      return;
    }
    h->compute_delete_jwt(
        "/api/v1/tema/library/collections/" + collection_id + "/movies",
        movie_id);
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      success("Film sters din colectie");
    } else {
      error(hres->get_error());
    }
  }

  void logout() {
    h->compute_get("/api/v1/tema/user/logout");
    if (DEBUG) cout << h->get_message() << endl;
    put_message();
    if (hres->check_response()) {
      h->clear_cookies();
      success("User delogat");
    } else {
      error(hres->get_error());
    }
  }

  void add_collection_routine() {
    string title = get_input_line("title");
    int rc = add_collection(title);
    if (rc < 0) {
      error("Failed to create collection");
      return;
    }

    string collection_id;
    rc = get_collection_by_title(title, collection_id);
    if (rc < 0) {
      error("Failed to create collection");
      return;
    }

    int num_movies;
    cout << "num_movies=";
    cin >> num_movies;

    for (int i = 0; i < num_movies; i++) {
      string movie_id = get_input("movie_id[" + to_string(i) + "]");
      rc = add_movie_to_collection(collection_id, movie_id);
      if (rc < 0) {
        error("Failed to create collection");
        return;
      }
    }

    get_collection(collection_id);
  }

  int get_collection_by_title(string title, string &id) {
    json j;
    int rc = get_collections(j);
    if (rc < 0) {
      return -1;
    }

    int found = -1;
    for (auto it : j["collections"]) {
      if (it["title"] == title) {
        id = to_string(it["id"]);
        found = 0;
        break;
      }
    }

    return found;
  }
};

int main() {
  Client c("63.32.125.183", 8081);
  c.run();
}
