#include <iostream>
#include <vector>
#include <cassert>
#include <string>
#include <cstring>

#define BUFFER_DEFAULT_SIZE 1024
class Buffer
{
public:
    Buffer()
        :_buffer(BUFFER_DEFAULT_SIZE)
        , _read_index(0)
        , _write_index(0)
    {}

    // 返回缓冲区的起始地址
    char* Begin() { return &(*_buffer.begin()); }

    // 获取当前读取位置起始地址
    char* GetReadPos() { return Begin() + _read_index; }

    // 获取当前写入位置起始地址（缓冲区起始地址加上写偏移量）
    char* GetWritePos() { return Begin() + _write_index; }

    // 获取缓冲区头部空闲空间大小--读偏移之前的空闲空间
    uint64_t HeadIdleSize() { return _read_index; }

    // 获取缓冲区末尾空闲空间大小--写偏移之后的空闲空间
    uint64_t TailIdleSize() { return _buffer.size() - _write_index; }

    // 获取可读数据大小
    uint64_t ReadableSize() { return _write_index - _read_index; }

    // 将读偏移向后移动
    void MoveReadOffset(uint64_t len)
    {
        assert(len <= ReadableSize()); // 向后移动的大小必须小于可读数据的大小
        _read_index += len;
    }

    // 将写偏移向后移动
    void MoveWriteOffset(uint64_t len)
    {
        assert(len <= TailIdleSize()); // 向后移动的大小必须小于缓冲区末尾空闲空间的大小
        _write_index += len;
    }

    // 确保可写空间足够（整体的空闲空间够存数据就将数据移动，否则扩容）
    void EnsureWriteSpace(uint64_t len)
    {
        // 若末尾空闲空间足够，则直接返回
        if (TailIdleSize() >= len) return;

        // 若末尾空闲空间不够，则判断末尾空闲空间加上头部空闲空间是否足够
        // 足够则将可读数据移动到缓冲区的起始位置，给后面留出完整的空间来写入数据
        if (HeadIdleSize() + TailIdleSize() >= len)
        {
            // 将可读数据移动到缓冲区起始位置
            uint64_t readsize = ReadableSize(); // 保存缓冲区当前可读数据的大小
            std::copy(GetReadPos(), GetReadPos() + readsize, Begin()); // 将可读数据拷贝到缓冲区起始位置
            _read_index = 0; // 将读偏移归零
            _write_index = readsize; // 将写偏移置为可读数据大小
        }
        else // 总体空间不够，则进行扩容，无需移动数据，直接在写偏移之后扩容足够的空间即可
        {
            _buffer.resize(_write_index + len);
        }
    }

    // 读取数据
    void Read(void* buffer, uint64_t len)
    {
        // 读取数据的大小必须小于可读数据大小
        assert(len <= ReadableSize());
        std::copy(GetReadPos(), GetReadPos() + len, (char*)buffer);
    }

    // 读取数据并将du偏移向后移动
    void ReadAndPop(void* buffer, uint64_t len)
    {
        Read(buffer, len);
        MoveReadOffset(len);
    }

    // 将数据作为字符串读取
    std::string ReadAsString(uint64_t len)
    {
        // 读取数据的大小必须小于可读数据大小
        assert(len <= ReadableSize());

        std::string str;
        str.resize(len);

        Read(&str[0], len);

        return str;
    }

    std::string ReadAsStringAndPop(uint64_t len)
    {
        std::string str = ReadAsString(len);
        MoveReadOffset(len);

        return str;
    }

    // 写入数据
    void Write(const void* data, uint64_t len)
    {
        // 1.保证缓冲区有足够的可写空间
        EnsureWriteSpace(len);

        // 2.拷贝数据
        const char* d = (const char*)data;
        std::copy(d, d + len, GetWritePos());
    }

    void WriteAndPush(const void* data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }

    // 往缓冲区中写入字符串
    void WriteString(const std::string& data)
    {
        return Write(data.c_str(), data.size());
    }

    void WriteStringAndPush(const std::string& data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }

    // 往缓冲区中写入另一个缓冲区的数据
    void WriteBuffer(Buffer& data)
    {
        return Write(data.GetReadPos(), data.ReadableSize());
    }

    void WriteBufferAndPush(Buffer& data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadableSize());
    }

    char* FindCRLF()
    {
        char* pos = (char*)memchr(GetReadPos(), '\n', ReadableSize());
        return pos;
    }

    std::string GetLine()
    {
        char* pos = FindCRLF();
        if (pos == NULL) return "";

        return ReadAsString(pos - GetReadPos() + 1); // +1是为了把'\n'也取出来
    }

    std::string GetLineAndPop()
    {
        std::string str = GetLine();
        MoveReadOffset(str.size());
        
        return str;
    }

    // 清空缓冲区
    void Clear()
    {
        // 只需要将两个偏移量归零即可
        _read_index = 0;
        _write_index = 0;
    }

private:
    std::vector<char> _buffer; // 使用vector进行内存管理
    uint64_t _read_index;      // 读偏移
    uint64_t _write_index;     // 写偏移
};