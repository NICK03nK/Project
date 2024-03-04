#pragma once

#include "../server.hpp"

#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex>

#define DEFAULT_TIMEOUT 30

class Util
{
public:
    // 字符串分割 ---- 将源字符串src按照sep进行分割，将分割得到的各个字符串放入array，最终返回子串的数量
    static size_t Split(const std::string& src, const std::string& sep, std::vector<std::string>* array)
    {
        size_t offset = 0;
        // abc,
        while (offset <= src.size())
        {
            size_t pos = src.find(sep, offset); // 在字符串src中从offset位置开始往后查找sep，找到返回sep最小下标
            if (pos == std::string::npos) // 从offset开始往后没有找到分隔字符串sep
            {
                // 将剩余部分当作整体，放入array中
                if (offset == src.size()) break;
                array->push_back(src.substr(offset));
                return array->size();
            }

            // 找到了分隔字符串sep
            if (pos - offset != 0)
            {
                array->push_back(src.substr(offset, pos - offset));
            }
            offset = pos + sep.size();
        }

        return array->size();
    }

    // 读取文件所有内容，将文件内容放到bu中
    static bool ReadFile(const std::string& filename, std::string* buf)
    {
        std::ifstream ifs(filename.c_str(), std::ios::binary);
        if (ifs.is_open() == false) // 打开文件失败
        {
            ERR_LOG("open %s file failed", filename.c_str());
            return false;
        }

        // 打开文件成功
        // 读取文件大小
        size_t fsize = 0;
        ifs.seekg(0, ifs.end); // 文件指针跳转到文件末尾
        fsize = ifs.tellg(); // 获取文件大小
        ifs.seekg(0, ifs.beg); // 文件指针跳转到文件开头

        // 将文件内容读取到str中
        buf->resize(fsize);
        ifs.read(&(*buf)[0], fsize);

        if (ifs.good() == false)
        {
            ERR_LOG("read %s file failed", filename.c_str());
            ifs.close();

            return false;
        }

        ifs.close();
        return true;
    }

    // 向文件中写入数据
    static bool WriteFile(const std::string& filename, const std::string& buf)
    {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (ofs.is_open() == false) // 打开文件失败
        {
            ERR_LOG("open %s file failed", filename.c_str());
            return false;
        }

        // 打开文件成功
        ofs.write(buf.c_str(), buf.size()); // 将buf中的内容写入ofs指向的文件中

        if (ofs.good() == false)
        {
            ERR_LOG("write %s file failed", filename.c_str());
            ofs.close();

            return false;
        }

        ofs.close();
        return true;
    }

    // URL编码
    static std::string URLEncode(const std::string& url, bool convert_space_to_plus)
    {
        std::string res;
        for (auto c : url)
        {
            if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c))
            {
                res += c;
                continue;
            }

            if (c == ' ' && convert_space_to_plus)
            {
                res += '+';
                continue;
            }

            // 剩下的字符要编码成 %HH 的格式
            char tmp[4] = { 0 };
            snprintf(tmp, 4, "%%%02X", c);
            res += tmp;
        }

        return res;
    }

    static char HEXTOI(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c >= 'a' && c <= 'z')
        {
            return c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'Z')
        {
            return c - 'A' + 10;
        }

        return -1;
    }

    // URL解码
    static std::string URLDecode(const std::string& url, bool convert_plus_to_space)
    {
        std::string res;
        for (int i = 0; i < url.size(); ++i)
        {
            if (url[i] == '+' && convert_plus_to_space)
            {
                res += ' ';
                continue;
            }

            if (url[i] == '%' && (i + 2) < url.size())
            {
                char v1 = HEXTOI(url[i + 1]);
                char v2 = HEXTOI(url[i + 2]);
                char v = (v1 << 4) + v2;
                res += v;
                i += 2;
                continue;
            }

            res += url[i];
        }

        return res;
    }

    // 获取响应状态码的描述信息
    static std::string StatuDesc(int statu)
    {
        std::unordered_map<int, std::string> _statu_msg = {
            {100, "Continue"},
            {101, "Switching Protocol"},
            {102, "Processing"},
            {103, "Early Hints"},
            {200, "OK"},
            {201, "Created"},
            {202, "Accepted"},
            {203, "Non-Authoritative Information"},
            {204, "No Content"},
            {205, "Reset Content"},
            {206, "Partial Content"},
            {207, "Multi-Status"},
            {208, "Already Reported"},
            {226, "IM Used"},
            {300, "Multiple Choice"},
            {301, "Moved Permanently"},
            {302, "Found"},
            {303, "See Other"},
            {304, "Not Modified"},
            {305, "Use Proxy"},
            {306, "unused"},
            {307, "Temporary Redirect"},
            {308, "Permanent Redirect"},
            {400, "Bad Request"},
            {401, "Unauthorized"},
            {402, "Payment Required"},
            {403, "Forbidden"},
            {404, "Not Found"},
            {405, "Method Not Allowed"},
            {406, "Not Acceptable"},
            {407, "Proxy Authentication Required"},
            {408, "Request Timeout"},
            {409, "Conflict"},
            {410, "Gone"},
            {411, "Length Required"},
            {412, "Precondition Failed"},
            {413, "Payload Too Large"},
            {414, "URI Too Long"},
            {415, "Unsupported Media Type"},
            {416, "Range Not Satisfiable"},
            {417, "Expectation Failed"},
            {418, "I'm a teapot"},
            {421, "Misdirected Request"},
            {422, "Unprocessable Entity"},
            {423, "Locked"},
            {424, "Failed Dependency"},
            {425, "Too Early"},
            {426, "Upgrade Required"},
            {428, "Precondition Required"},
            {429, "Too Many Requests"},
            {431, "Request Header Fields Too Large"},
            {451, "Unavailable For Legal Reasons"},
            {501, "Not Implemented"},
            {502, "Bad Gateway"},
            {503, "Service Unavailable"},
            {504, "Gateway Timeout"},
            {505, "HTTP Version Not Supported"},
            {506, "Variant Also Negotiates"},
            {507, "Insufficient Storage"},
            {508, "Loop Detected"},
            {510, "Not Extended"},
            {511, "Network Authentication Required"}
        };

        auto it = _statu_msg.find(statu);
        if (it != _statu_msg.end())
        {
            return it->second;
        }

        return "Unkonw";
    }

    // 根据文件后缀名获取mime
    static std::string ExtMime(const std::string& filename)
    {
        std::unordered_map<std::string, std::string> _mime_msg = {
            {".aac",        "audio/aac"},
            {".abw",        "application/x-abiword"},
            {".arc",        "application/x-freearc"},
            {".avi",        "video/x-msvideo"},
            {".azw",        "application/vnd.amazon.ebook"},
            {".bin",        "application/octet-stream"},
            {".bmp",        "image/bmp"},
            {".bz",         "application/x-bzip"},
            {".bz2",        "application/x-bzip2"},
            {".csh",        "application/x-csh"},
            {".css",        "text/css"},
            {".csv",        "text/csv"},
            {".doc",        "application/msword"},
            {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
            {".eot",        "application/vnd.ms-fontobject"},
            {".epub",       "application/epub+zip"},
            {".gif",        "image/gif"},
            {".htm",        "text/html"},
            {".html",       "text/html"},
            {".ico",        "image/vnd.microsoft.icon"},
            {".ics",        "text/calendar"},
            {".jar",        "application/java-archive"},
            {".jpeg",       "image/jpeg"},
            {".jpg",        "image/jpeg"},
            {".js",         "text/javascript"},
            {".json",       "application/json"},
            {".jsonld",     "application/ld+json"},
            {".mid",        "audio/midi"},
            {".midi",       "audio/x-midi"},
            {".mjs",        "text/javascript"},
            {".mp3",        "audio/mpeg"},
            {".mpeg",       "video/mpeg"},
            {".mpkg",       "application/vnd.apple.installer+xml"},
            {".odp",        "application/vnd.oasis.opendocument.presentation"},
            {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
            {".odt",        "application/vnd.oasis.opendocument.text"},
            {".oga",        "audio/ogg"},
            {".ogv",        "video/ogg"},
            {".ogx",        "application/ogg"},
            {".otf",        "font/otf"},
            {".png",        "image/png"},
            {".pdf",        "application/pdf"},
            {".ppt",        "application/vnd.ms-powerpoint"},
            {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
            {".rar",        "application/x-rar-compressed"},
            {".rtf",        "application/rtf"},
            {".sh",         "application/x-sh"},
            {".svg",        "image/svg+xml"},
            {".swf",        "application/x-shockwave-flash"},
            {".tar",        "application/x-tar"},
            {".tif",        "image/tiff"},
            {".tiff",       "image/tiff"},
            {".ttf",        "font/ttf"},
            {".txt",        "text/plain"},
            {".vsd",        "application/vnd.visio"},
            {".wav",        "audio/wav"},
            {".weba",       "audio/webm"},
            {".webm",       "video/webm"},
            {".webp",       "image/webp"},
            {".woff",       "font/woff"},
            {".woff2",      "font/woff2"},
            {".xhtml",      "application/xhtml+xml"},
            {".xls",        "application/vnd.ms-excel"},
            {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
            {".xml",        "application/xml"},
            {".xul",        "application/vnd.mozilla.xul+xml"},
            {".zip",        "application/zip"},
            {".3gp",        "video/3gpp"},
            {".3g2",        "video/3gpp2"},
            {".7z",         "application/x-7z-compressed"}
        };

        // 获取文件扩展名
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos)
        {
            return "application/octet-stream";
        }

        // 根据文件扩展名获取mime
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end())
        {
            return "application/octet-stream";
        }

        return it->second;
    }

    // 判断一个文件是否是目录
    static bool IsDirectory(const std::string& filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }

        return S_ISDIR(st.st_mode);
    }

    // 判断一个文件是否是普通文件
    static bool IsRegular(const std::string& filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }

        return S_ISREG(st.st_mode);
    }

    // http请求的资源路径有效性判断
    static bool ValidPath(const std::string& path)
    {
        std::vector<std::string> subdir;
        Split(path, "/", &subdir);

        int level = 0;
        for (const auto& dir : subdir)
        {
            if (dir == "..")
            {
                --level;
                if (level < 0)
                {
                    return false;
                }

                continue;
            }
            ++level;
        }

        return true;
    }
};

class HttpRequest
{
public:
    HttpRequest() : _version("HTTP/1.1") {}

    // 插入头部字段
    void SetHeader(const std::string& key, const std::string& val)
    {
        _headers.insert(std::make_pair(key, val));
    }

    // 判断是否存在指定的头部字段
    bool HasHeader(const std::string& key) const
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return false;
        }

        return true;
    }

    // 获取指定的头部字段的值
    std::string GetHeader(const std::string& key) const
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }

        return it->second;
    }

    // 插入查询字符串
    void SetParam(const std::string& key, const std::string& val)
    {
        _params.insert(std::make_pair(key, val));
    }

    // 判断是否存在指定的查询字符串
    bool HasParam(const std::string& key) const
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return false;
        }

        return true;
    }

    // 获取指定的查询字符串
    std::string GetParam(const std::string& key) const
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return "";
        }

        return it->second;
    }

    // 获取正文长度
    size_t ContentLength() const
    {
        bool ret = HasHeader("Content-Length");
        if (ret == false)
        {
            return 0;
        }

        std::string clen = GetHeader("Content-Length");
        return std::stol(clen);
    }

    // 判断是否是短链接
    bool Close() const
    {
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false; // 长连接
        }

        return true; // 短链接
    }

    // 重置
    void ReSet()
    {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _body.clear();
        std::smatch match;
        _match.swap(match);
        _headers.clear();
        _params.clear();
    }

public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 资源路径
    std::string _version;                                  // 协议版本
    std::string _body;                                     // 请求正文
    std::smatch _match;                                    // 资源路径的正则提取数据
    std::unordered_map<std::string, std::string> _headers; // 头部字段
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
};

class HttpResponse
{
public:
    HttpResponse()
        :_statu(200)
        , _redirect_flag(false)
    {}

    HttpResponse(int statu)
        :_statu(statu)
        , _redirect_flag(false)
    {}

    void ReSet()
    {
        _statu = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }

    // 插入头部字段
    void SetHeader(const std::string& key, const std::string& val)
    {
        _headers.insert(std::make_pair(key, val));
    }

    // 判断是否存在指定的头部字段
    bool HasHeader(const std::string& key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return false;
        }

        return true;
    }

    // 获取指定的头部字段的值
    std::string GetHeader(const std::string& key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }

        return it->second;
    }

    // 设置正文
    void SetContent(const std::string& body, const std::string& type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }

    // 设置重定向
    void SetRedirect(const std::string& url, int statu = 302)
    {
        _statu = statu;
        _redirect_flag = true;
        _redirect_url = url;
    }

    // 判断是否是短链接
    bool Close()
    {
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false; // 长连接
        }

        return true; // 短链接
    }

public:
    int _statu;                                            // 响应状态码
    bool _redirect_flag;                                   // 重定向标志
    std::string _body;                                     // 正文
    std::string _redirect_url;                             // 重定向url
    std::unordered_map<std::string, std::string> _headers; // 头部字段
};

typedef enum
{
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
} HttpRecvStatu;

#define MAX_LINE 8192
class HttpContext
{
public:
    HttpContext()
        :_resp_statu(200)
        , _recv_statu(RECV_HTTP_LINE)
    {}

    void ReSet()
    {
        _resp_statu = 200;
        _recv_statu = RECV_HTTP_LINE;
        _request.ReSet();
    } 

    int RespStatu() { return _resp_statu; }

    HttpRecvStatu RecvStatu() { return _recv_statu; }

    HttpRequest& Request() { return _request; }
    
    // 接收并解析HTTP请求
    void RecvHttpRequest(Buffer* buf)
    {
        switch (_recv_statu)
        {
        case RECV_HTTP_LINE:
            RecvHttpLine(buf);
        case RECV_HTTP_HEAD:
            RecvHttpHead(buf);
        case RECV_HTTP_BODY:
            RecvHttpBody(buf);
        }

        return;
    }

private:
    bool RecvHttpLine(Buffer* buf)
    {
        if (_recv_statu != RECV_HTTP_LINE) return false;
        
        // 1.获取一行数据
        std::string line = buf->GetLineAndPop();
        
        // 2.需要考虑两个因素：缓冲区的数据不足一行/获取一行数据的大小很大
        if (line.size() == 0)
        {
            // 缓冲区中的数据不足一行，则需要判断缓冲区中可读数据的长度，若可读数据很长了都不足一行，那就是出问题了
            if (buf->ReadableSize() > MAX_LINE)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414; // URI TOO LONG
                return false;
            }

            // 缓冲区中数据不足一行，但也不多，那就等后续数据的到来
            return true;
        }

        if (line.size() > MAX_LINE)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 414; // URI TOO LONG
            return false;
        }

        bool ret = ParseHttpLine(line);
        if (ret == false)
        {
            return false;
        }

        // 获取首行完毕，进入获取头部字段阶段
        _recv_statu = RECV_HTTP_HEAD;
        return true;
    }

    bool ParseHttpLine(const std::string& line)
    {
        std::smatch matches;
        std::regex e("(GET|POST|PUT|HEAD|DELETE|OPTIONS|TRACE|CONNECT|LINK|UNLINE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        bool ret = std::regex_match(line, matches, e);
        if (ret == false)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400; // BAD REQUEST
            return false;
        }

        // 0 : GET /home/login?user=nk&pass=123123 HTTP/1.1
        // 1 : GET
        // 2 : /home/login
        // 3 : user=nk&pass=123123
        // 4 : HTTP/1.1

        // 请求方法的获取
        _request._method = matches[1];
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper); // 将请求行中的请求方法全部改成大写

        // 资源路径的获取，需要进行URL的解码操作，但不需要将+转为空格
        _request._path = Util::URLDecode(matches[2], false);

        // 协议版本的获取
        _request._version = matches[4];

        // 查询字符串的获取与处理
        std::vector<std::string> query_string_array;
        std::string query_string = matches[3]; // 获取查询字符串

        // 查询字符串的格式为：key=val&key=val...，先以&进行分隔，得到各个子串，格式为：key=val
        Util::Split(query_string, "&", &query_string_array);

        // 针对各个子串，以=进行分隔，得到key和val，对key和val也要进行解码操作
        for (auto& str : query_string_array)
        {
            size_t pos = str.find("=");
            if (pos == std::string::npos) // 在子串中没找到=，出错返回
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 400; // BAD REQUEST
                return false;
            }

            // 获取key和val
            std::string key = Util::URLDecode(str.substr(0, pos), true);
            std::string val = Util::URLDecode(str.substr(pos + 1), true);

            // 插入查询字符串
            _request.SetParam(key, val);
        }

        return true;
    }

    bool RecvHttpHead(Buffer* buf)
    {
        if(_recv_statu != RECV_HTTP_HEAD) return false;
        // 一行一行取出数据，直到遇到空行为止，头部字段的格式为：key: val\r\nkey: val\r\n...

        while (true) // 在读取到空行之前要一直循环读取头部字段
        {
            // 1.获取一行数据
            std::string line = buf->GetLineAndPop();
            
            // 2.需要考虑两个因素：缓冲区的数据不足一行/获取一行数据的大小很大
            if (line.size() == 0)
            {
                // 缓冲区中的数据不足一行，则需要判断缓冲区中可读数据的长度，若可读数据很长了都不足一行，那就是出问题了
                if (buf->ReadableSize() > MAX_LINE)
                {
                    _recv_statu = RECV_HTTP_ERROR;
                    _resp_statu = 414; // URI TOO LONG
                    return false;
                }

                // 缓冲区中数据不足一行，但也不多，那就等后续数据的到来
                return true;
            }

            if (line.size() > MAX_LINE)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414; // URI TOO LONG
                return false;
            }

            if (line == "\n" || line == "\r\n") // 读到了空行，获取所有头部字段完毕
            {
                break;
            }

            bool ret = ParseHttpHead(line);
            if (ret == false)
            {
                return false;
            }
        }

        // 获取头部完毕，进入获取正文阶段
        _recv_statu = RECV_HTTP_BODY;
        return true;
    }

    bool ParseHttpHead(const std::string& line)
    {
        // 头部字段的格式：key: val\r\n
        size_t pos = line.find(": ");
        if (pos == std::string::npos)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400; // BAD REQUEST
            return false;
        }

        // 获取key和val
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 2);

        // 插入头部字段
        _request.SetHeader(key, val);

        return true;
    }

    bool RecvHttpBody(Buffer* buf)
    {
        if (_recv_statu != RECV_HTTP_BODY) return false;

        // 1.获取正文长度
        size_t content_lenth = _request.ContentLength();
        if (content_lenth == 0) // 没有正文，则请求接收解析完毕
        {
            _recv_statu = RECV_HTTP_OVER;
            return true;
        }

        // 2.当前已经接收到多少正文，其实就是往_request._body中放了多少数据
        size_t real_len = content_lenth - _request._body.size(); // 实际还需要接收的长度

        // 3.接收正文放到body中，当时也要考虑当前缓冲区的数据是否是全部正文
        //    3.1.缓冲区中的数据包含了当前请求的所有正文，则取出所需的数据
        if (buf->ReadableSize() >= real_len)
        {
            _request._body.append(buf->GetReadPos(), real_len);
            buf->MoveReadOffset(real_len);

            _recv_statu = RECV_HTTP_OVER;
            return true;
        }

        //    3.2.缓冲区中的数据无法满足当前正文的完整性，数据不足，则取出数据，等待新数据的到来
        _request._body.append(buf->GetReadPos(), buf->ReadableSize());
        buf->MoveReadOffset(buf->ReadableSize());

        return true;
    }

private:
    int _resp_statu;           // 响应状态码
    HttpRecvStatu _recv_statu; // 当前接收及解析的阶段状态
    HttpRequest _request;      // 已经解析得到的请求信息
};

class HttpServer
{
    using Handler = std::function<void(const HttpRequest&, HttpResponse*)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
public:
    HttpServer(int port, int timeout = DEFAULT_TIMEOUT)
        :_server(port)
    {
        _server.EnableInactiveRelease(timeout); // 启用非活跃连接超时销毁功能，默认为30s
        _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }

    // 设置静态资源根目录
    void SetBaseDir(const std::string& path)
    {
        assert(Util::IsDirectory(path) == true);
        _basedir = path;
    }

    // 设置GET请求与对应处理函数的映射关系
    void Get(const std::string& pattern, const Handler& handler)
    {
        _get_route.push_back(make_pair(std::regex(pattern), handler));
    }

    // 设置POST请求与对应处理函数的映射关系
    void Post(const std::string& pattern, const Handler& handler)
    {
        _post_route.push_back(make_pair(std::regex(pattern), handler));
    }

    // 设置PUT请求与对应处理函数的映射关系
    void Put(const std::string& pattern, const Handler& handler)
    {
        _put_route.push_back(make_pair(std::regex(pattern), handler));
    }

    // 设置DELETE请求与对应处理函数的映射关系
    void Delete(const std::string& pattern, const Handler& handler)
    {
        _delete_route.push_back(make_pair(std::regex(pattern), handler));
    }

    // 设置线程数量
    void SetThreadCount(int count) { _server.SetThreadCount(count); }

    // 监听
    void Listen() { _server.Start(); }

private:
    // 将HttpResponse中的要素按照HTTP协议格式进行组织，发回
    void WriteResponse(const SharedConnection& conn, HttpRequest& req, HttpResponse& resp)
    {
        // 1.先完善头部字段
        if (req.Close() == true)
        {
            resp.SetHeader("Connection", "close");
        }
        else
        {
            resp.SetHeader("Connection", "keep-alive");
        }

        if (resp._body.empty() == false && resp.HasHeader("Content-Length") == false) // 响应正文不为空，但响应的头部字段中没有正文长度
        {
            resp.SetHeader("Content-Length", std::to_string(resp._body.size()));
        }

        if (resp._body.empty() == false && resp.HasHeader("Content-Type") == false) // 响应正文不为空，但响应的头部字段中没有正文类型
        {
            resp.SetHeader("Content-Type", "application/octet-stream");
        }

        if (resp._redirect_flag == true)
        {
            resp.SetHeader("Location", resp._redirect_url);
        }

        // 2.将resp中的要素按照HTTP协议格式进行组织
        std::stringstream resp_str;
        // 组织响应首行
        resp_str << req._version << " " << std::to_string(resp._statu) << " " << Util::StatuDesc(resp._statu) << "\r\n";
        
        // 组织响应头部字段
        for (const auto& it : resp._headers)
        {
            resp_str << it.first << ": " << it.second << "\r\n";
        }
        resp_str << "\r\n"; // 添加空行

        // 组织响应正文
        resp_str << resp._body;

        // 3.发送数据给客户端
        conn->Send(resp_str.str().c_str(), resp_str.str().size());
    }

    // 静态资源的请求处理
    void FileHandler(const HttpRequest& req, HttpResponse* resp)
    {
        std::string req_path = _basedir + req._path;
        if (req._path.back() == '/')
        {
            req_path += "index.html";
        }

        bool ret = Util::ReadFile(req_path, &resp->_body);
        if (ret == false) return;

        std::string mime = Util::ExtMime(req_path);
        resp->SetHeader("Content-Type", mime);

        return;
    }

    // 功能性请求的分类处理
    void Dispatcher(HttpRequest& req, HttpResponse* resp, Handlers& handlers)
    {
        // 在对应请求方法路由表中查找是否存在对应的请求处理函数，有则调用该函数，没有则返回404
        // handlers的本质是一个unordered_map，存放请求和对应的处理函数的映射关系

        // 思想：路由表存储的键值对 ---- 正则表达式 & 处理函数
        // 使用正则表达式对请求的资源路径进行正则匹配，匹配成功就调用对应函数进行处理
        for (auto& handler : handlers)
        {
            const std::regex& re = handler.first;
            const Handler& functor = handler.second;

            bool ret = std::regex_match(req._path, req._match, re);
            if(ret == false) // 正则匹配失败
            {
                continue; // 换一个路由请求表继续匹配
            }

            return functor(req, resp); // 传入请求信息和空的resp，执行处理函数
        }

        resp->_statu = 404; // 所有的路由表都没有匹配的请求处理函数，则返回404
    }

    // 判断是否是静态资源请求
    bool IsFileHandler(const HttpRequest& req)
    {
        // 1.必须设置静态资源根目录
        if (_basedir.empty()) return false;

        // 2.请求方法必须是GET/HEAD
        if (req._method != "GET" && req._method != "HEAD") return false;

        // 3.请求的资源路径必须是一个合法的路径
        if (Util::ValidPath(req._path) == false) return false;

        // 4.请求的资源必须存在，且必须是一个普通文件
        std::string req_path = _basedir + req._path;

        //   特殊情况：资源路径结尾是根目录的 -- 目录：/  /image/，这种情况默认在结尾追加一个index/html
        if (req._path.back() == '/')
        {
            req_path += "index.html";
        }

        // 判断资源路径是否是一个普通文件
        if (Util::IsRegular(req_path) == false) return false;

        return true;
    }

    void Route(HttpRequest& req, HttpResponse* resp)
    {
        // 1.对请求继续分辨，判断是静态资源请求还是功能性请求
        //   静态资源请求，则进行静态资源的处理
        //   功能性请求，则需要通过几个路由表来确定是否有对应的处理函数
        //   既不是静态资源请求，也没有设置对应的功能性请求处理函数，则返回405

        if (IsFileHandler(req) == true)
        {
            return FileHandler(req, resp);
        }

        if (req._method == "GET" || req._method == "HEAD")
        {
            return Dispatcher(req, resp, _get_route);
        }
        else if (req._method == "POST")
        {
            return Dispatcher(req, resp, _post_route);
        }
        else if (req._method == "PUT")
        {
            return Dispatcher(req, resp, _put_route);
        }
        else if (req._method == "DELETE")
        {
            return Dispatcher(req, resp, _delete_route);
        }

        resp->_statu = 405; // Method Not Allowed
    }

    // 设置上下文
    void OnConnected(const SharedConnection& conn)
    {
        conn->SetContext(HttpContext());
        DBG_LOG("new connection %p", conn->GetContext()); // ???????????????
    }

    void ErrorHandler(HttpResponse* resp)
    {
        // 1.组织一个错误的展示页面
        std::string body;
        body += "<html>";
        body += "<head>";
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        body += std::to_string(resp->_statu);
        body += " ";
        body += Util::StatuDesc(resp->_statu);
        body += "</h1>";
        body += "</body>";
        body += "</html>";

        // 2.将页面数据，当作响应正文，放入resp中
        resp->SetContent(body, "text/html");
    }

    // 缓冲区数据解析+处理
    void OnMessage(const SharedConnection& conn, Buffer* buf)
    {
        while (buf->ReadableSize() > 0) // 只要缓冲区中有数据，解析+处理就是一个循环处理的过程
        {
            // 1.获取上下文
            HttpContext* context = conn->GetContext()->get<HttpContext>();

            // 2.通过上下文对缓冲区数据进行解析，得到HttpRequest对象
            // 2.1.若缓冲区的数据解析出错，则直接回复出错响应
            // 2.2.若解析正常，且请求已经获取完毕，才开始去进行处理
            context->RecvHttpRequest(buf);
            HttpRequest& req = context->Request();
            HttpResponse resp(context->RespStatu());

            if (context->RespStatu() >= 400)
            {
                // 进行错误响应，关闭连接
                ErrorHandler(&resp); // 填充一个错误显示页面到resp中
                WriteResponse(conn, req, resp); // 组织响应发送给客户端
                conn->Shutdown();
                return;
            }

            if (context->RecvStatu() != RECV_HTTP_OVER)
            {
                // 当前请求还没有接收完整，先返回等新数据的到来再重新处理
                return;
            }

            // 3.请求路由 + 业务处理
            Route(req, &resp);

            // 4.对HttpResponse进行组织发送
            WriteResponse(conn, req, resp);

            // 5.重置上下文
            context->ReSet();

            // 6.根据长短连接判断是否关闭连接或者继续处理
            if (resp.Close() == true) conn->Shutdown(); // 短连接则直接关闭
        }

        return;
    }

private:
    Handlers _get_route;    // GET请求的路由映射表
    Handlers _post_route;   // POST请求的路由映射表
    Handlers _put_route;    // PUT请求的路由映射表
    Handlers _delete_route; // DELETE请求的路由映射表
    std::string _basedir;   // 静态资源路径
    TcpServer _server;      // tcp服务器
};