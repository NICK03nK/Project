#include <iostream>
#include <string>
#include <mysql/mysql.h>

#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PASSWD ""
#define DBNAME "MySQL_API_study"

int main()
{
    // 1. 初始化MySQL句柄
    MYSQL* mysql = mysql_init(NULL);
    if (mysql == NULL)
    {
        std::cout << "MySQL init failed!" << std::endl;
        return -1;
    }

    // 2. 连接服务器
    if (mysql_real_connect(mysql, HOST, USER, PASSWD, DBNAME, PORT, NULL, 0) == NULL)
    {
        std::cout << "connect MySQL server failed! " << mysql_error(mysql) << std::endl;
        mysql_close(mysql); // 退出前断开连接，释放mysql操作句柄
        return -1;
    }

    // 3. 设置客户端字符集
    if (mysql_set_character_set(mysql, "utf8") != 0)
    {
        std::cout << "set client character failed! " << mysql_error(mysql) << std::endl;
        mysql_close(mysql); // 退出前断开连接，释放mysql操作句柄
        return -1;
    }

    // 4. 选择要操作的数据库（这一步在连接MySQL服务器时，函数参数中已经设置过了）

    // 5. 执行SQL语句
    // const char* sql = "insert into student values(null, 'nK', 21, 99.3, 100, 89.5);";
    // const char* sql = "update student set chinese=chinese + 30 where sn=1;";
    // const char* sql = "delete from student where sn=1;";
    const char* sql = "select * from student";
    int ret = mysql_query(mysql, sql);
    if (ret != 0)
    {
        std::cout << "mysql query failed! " << mysql_error(mysql) << std::endl;
        mysql_close(mysql); // 退出前断开连接，释放mysql操作句柄
        return -1;
    }

    // 6. 若SQL语句是查询语句，则将查询结果保存到本地
    MYSQL_RES* res = mysql_store_result(mysql);
    if (res == NULL)
    {
        mysql_close(mysql);
        return -1;
    }
    
    // 7. 获取结果集中的结果条数
    int row = mysql_num_rows(res);
    int col = mysql_num_fields(res);

    // 8. 遍历保存到本地的结果集
    for (int i = 0; i < row; ++i)
    {
        MYSQL_ROW line = mysql_fetch_row(res);
        for (int j = 0; j < col; ++j)
        {
            std::cout << line[j] << "\t";
        }
        std::cout << std::endl;
    }

    // 9. 释放结果集
    mysql_free_result(res);

    // 10. 关闭连接，释放句柄
    mysql_close(mysql);

    return 0;
}