#include <stdio.h>
#include <string.h>
#include <string>
#include <mysql/mysql.h>

#define HOST "127.0.0.1"
#define USER "root"
#define PASSWD "N18124665842@"
#define DB "gobang"
#define PORT 3306

int main()
{
    // 1.初始化mysql句柄
    MYSQL* mysql = mysql_init(NULL);
    if (mysql == NULL)
    {
        printf("mysql init failed\n");
        return -1;
    }

    // 2.连接mysql服务器
    if (mysql_real_connect(mysql, HOST, USER, PASSWD, DB, PORT, NULL, 0) == NULL)
    {
        printf("connect mysql server failed: %s\n", mysql_error(mysql));
        mysql_close(mysql); // 出错退出前要断开连接销毁句柄
        
        return -1;
    }

    // 3.设置客户端字符集
    if (mysql_set_character_set(mysql, "utf8") != 0)
    {
        printf("set client character failed: %s\n", mysql_error(mysql));
        mysql_close(mysql); // 出错退出前要断开连接销毁句柄
        return -1;
    }

    // 4.选择要操作的数据库
    if (mysql_select_db(mysql, DB) != 0)
    {
        printf("select database failed: %s\n", mysql_error(mysql));
        mysql_close(mysql); // 出错退出前要断开连接销毁句柄
        return -1;
    }

    // 5.执行sql语句
    // std::string sql = "insert stu values(null, 'zhangsan', 20, 112, 139, 123);";
    // std::string sql = "update stu set chinese=chinese+10 where sn=1;";
    std::string sql = "select * from stu;";
    int ret = mysql_query(mysql, sql.c_str());
    if (ret != 0)
    {
        printf("%s\n", sql);
        printf("mysql query failed: %s\n", mysql_error(mysql));
        mysql_close(mysql); // 出错退出前要断开连接销毁句柄
        return -1;
    }

    // 6.如果是查询语句，则需要将查询结果保存到本地
    MYSQL_RES* res = mysql_store_result(mysql);
    if (res == NULL)
    {
        mysql_close(mysql); // 出错退出前要断开连接销毁句柄
        return -1;
    }
    
    // 7.获取结果集中的结果条数
    uint64_t num_rows = mysql_num_rows(res);
    unsigned int num_cols = mysql_num_fields(res);

    // 8.遍历保存到本地的结果集
    for (int i = 0; i < num_rows; ++i)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        for (int j = 0; j < num_cols; ++j)
        {
            printf("%s\t", row[j]);
        }
        printf("\n");
    }

    // 9.释放结果集
    mysql_free_result(res);

    // 10.关闭连接，释放句柄
    mysql_close(mysql);

    return 0;
}