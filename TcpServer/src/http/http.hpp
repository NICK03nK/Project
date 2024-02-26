#pragma once

#include "../server.hpp"

#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex>

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

    // 插入查询字符串
    void SetParam(const std::string& key, const std::string& val)
    {
        _params.insert(std::make_pair(key, val));
    }

    // 判断是否存在指定的查询字符串
    bool HasParam(const std::string& key)
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return false;
        }

        return true;
    }

    // 获取指定的查询字符串
    std::string GetParam(const std::string& key)
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return "";
        }

        return it->second;
    }

    // 获取正文长度
    size_t ContentLength()
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
    bool Close()
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
        _verison.clear();
        _body.clear();
        std::smatch match;
        _match.swap(match);
        _headers.clear();
        _params.clear();
    }

public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 资源路径
    std::string _verison;                                  // 协议版本
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

private:
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
        buf->MoveReadOffset(line.size()); // 获取HTTP请求首行成功，跳过HTTP请求的首行

        // 获取首行完毕，进入获取头部字段阶段
        _recv_statu = RECV_HTTP_HEAD;
        return true;
    }

    bool ParseHttpLine(const std::string& line)
    {
        std::smatch matches;
        std::regex e("(GET|POST|PUT|HEAD|DELETE|OPTIONS|TRACE|CONNECT|LINK|UNLINE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?");
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

        // 资源路径的获取，需要进行URL的解码操作，但不需要将+转为空格
        _request._path = Util::URLDecode(matches[2], false);

        // 协议版本的获取
        _request._verison = matches[4];

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
        if (buf->ReadableSize() > real_len)
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