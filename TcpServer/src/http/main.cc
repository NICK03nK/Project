#include "http.hpp"

#define WWWROOT "./wwwroot/"

std::string RequestStr(const HttpRequest& req)
{
    std::stringstream ss;

    ss << req._method << " " << req._path << " " << req._version << "\r\n";

    for (auto& it : req._params)
    {
        ss << it.first << ": " << it .second << "\r\n";
    }

    for (auto& it : req._headers)
    {
        ss << it.first << ": " << it.second << "\r\n";
    }

    ss << "\r\n";
    ss << req._body;

    return ss.str();
}

void Hello(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req), "text/plain");
}

void Login(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req), "text/plain");
}

void PutFile(const HttpRequest& req, HttpResponse* resp)
{
    std::string pathname = WWWROOT + req._path;
    Util::WriteFile(pathname, req._body);
}

void DeleteFile(const HttpRequest& req, HttpResponse* resp)
{
    resp->SetContent(RequestStr(req), "text/plain");
}

int main()
{
    HttpServer server(8080);
    server.SetThreadCount(3);
    server.SetBaseDir(WWWROOT);
    server.Get("/hello", Hello);
    server.Post("/login", Login);
    server.Put("/1234.txt", PutFile);
    server.Delete("/1234.txt", DeleteFile);
    server.Listen();

    return 0;
}