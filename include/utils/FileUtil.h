#pragma once 

#include <fstream>
#include <string>
#include <vector>

#include <muduo/base/Logging.h>

class FileUtil
{
public:
    FileUtil(std::string filePath)
        : filePath_(filePath)
        , file_(filePath, std::ios::binary)  // 打开文件，二进制模式
    {}

    ~FileUtil()
    {
        file_.close();
    }

    // 判断是否是有效路径
    bool isValid() const
    { return file_.is_open(); }

    // 重置打开默认文件
    void resetDefaultFile()
    {
        file_.close();
        file_.open("", std::ios::binary);  // 这里还没有写正确的默认文件路径
    }

    uint64_t size()
    {
        file_.seekg(0, std::ios::end); 
        // 上面这一行的意思是：从末尾往后0字节，也就是定位到文件的末尾
        // seekg 是移动读指针，seekp是移动写指针
        uint64_t fileSize = file_.tellg(); // 上一条先把读指针移动到文件尾部，然后这一条查询读指针的位置，这样就能获取大小了
        file_.seekg(0, std::ios::beg); // 返回到文件开头
        return fileSize;
    }

    void readFile(std::vector<char>& buffer)
    {
        // 会调用内部的 operator bool() 函数
        if(file_.read(buffer.data(), size()))
        {
            LOG_INFO << "File content load into memory (" << size() << " bytes)";
        }
        else{
            LOG_ERROR << "File read failed";
        }
    }

private:
    std::string             filePath_;
    std::ifstream           file_;
};