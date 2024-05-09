#pragma once

#include <vector>
#include <jsoncpp/json/json.h>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdlib>
#include <ctime>

#include "logger.hpp"
#include "db.hpp"
#include "online.hpp"
#include "util.hpp"

#define BOARD_ROWS 15
#define BOARD_COLS 15
#define WHITE_CHESS 1
#define BLACK_CHESS 2

enum room_statu
{
    GAME_START,
    GAME_OVER
};

class room
{
public:
    room(uint64_t room_id, user_table* user_table, online_manager* user_online)
        :_room_id(room_id)
        , _statu(GAME_START)
        , _player_count(0)
        , _user_table(user_table)
        , _user_online(user_online)
        , _board(BOARD_ROWS, std::vector<int>(BOARD_COLS, 0))
    {
        DLOG("%lu 房间创建成功", _room_id);
    }

    ~room() { DLOG("%lu 房间销毁成功", _room_id); }

    // 获取游戏房间id
    uint64_t id() { return _room_id; }

    // 获取游戏房间状态
    room_statu statu() { return _statu; }

    // 获取游戏房间的玩家数量
    int player_count() { return _player_count; }

    // 添加白棋玩家
    void add_white_player(uint64_t user_id)
    {
        _white_id = user_id;
        ++_player_count;
    }

    // 添加黑棋玩家
    void add_black_player(uint64_t user_id)
    {
        _black_id = user_id;
        ++_player_count;
    }

    // 获取白棋玩家id
    uint64_t get_white_player() { return _white_id; }

    // 获取黑棋玩家id
    uint64_t get_black_player() { return _black_id; }

    // 处理下棋动作
    Json::Value handle_chess(const Json::Value& req)
    {
        Json::Value resp;
        
        uint64_t cur_user_id = req["uid"].asUInt64();
        int chess_row = req["row"].asInt();
        int chess_col = req["col"].asInt();

        // 1. 判断走棋位置是否合理（是否越界，是否被占用）
        if (chess_row >= BOARD_ROWS || chess_col >= BOARD_COLS)
        {
            resp["optype"] = "put_chess";
            resp["result"] = false;
            resp["reason"] = "下棋位置越界";

            return resp;
        }
        else if (_board[chess_row][chess_col] != 0)
        {
            resp["optype"] = "put_chess";
            resp["result"] = false;
            resp["reason"] = "下棋位置被占用";

            return resp;
        }

        resp = req;

        // 2. 判断房间中两个玩家是否在线，若有一个退出，则判另一个获胜
        // 判断白棋玩家是否在线
        if (_user_online->is_in_game_room(_white_id) == false) // 白棋玩家掉线
        {
            resp["result"] = true;
            resp["reason"] = "白棋玩家掉线，黑棋玩家获胜";
            resp["winner"] = (Json::UInt64)_black_id;

            return resp;
        }
        // 判断黑棋玩家是否在线
        if (_user_online->is_in_game_room(_black_id) == false) // 黑棋玩家掉线
        {
            resp["result"] = true;
            resp["reason"] = "黑棋玩家掉线，白棋玩家获胜";
            resp["winner"] = (Json::UInt64)_white_id;

            return resp;
        }

        // 3. 下棋
        int cur_chess_color = cur_user_id == _white_id ? WHITE_CHESS : BLACK_CHESS;
        _board[chess_row][chess_col] = cur_chess_color;

        // 4. 判断是否有玩家胜利(从当前走棋位置开始判断，是否存在五星连珠)
        uint64_t winner_id = check_win(chess_row, chess_col, cur_chess_color);
        if (winner_id != 0) // winner_id 等于0表示没有玩家获胜
        {
            std::string reason = winner_id == _white_id ? "白棋五星连珠，白棋获胜，游戏结束" : "黑棋五星连珠，黑棋获胜，游戏结束";
            resp["result"] = true;
            resp["reason"] = reason;
            resp["winner"] = (Json::UInt64)winner_id;

            return resp;
        }

        // 没有玩家获胜，正常走棋
        resp["result"] = true;
        resp["reason"] = "正常走棋，游戏继续";
        resp["winner"] = (Json::UInt64)winner_id;

        return resp;
    }

    // 处理聊天动作
    Json::Value handle_chat(const Json::Value& req)
    {
        Json::Value resp;

        // 检测消息中是否包含敏感词
        std::string msg = req["message"].asString();
        if (have_sensitive_word(msg))
        {
            resp["optype"] = "chat";
            resp["result"] = false;
            resp["reason"] = "消息中包含敏感词";

            return resp;
        }

        resp = req;
        resp["result"] = true;

        return resp;
    }

    // 处理玩家退出房间动作
    void handle_exit(uint64_t user_id)
    {
        Json::Value resp;

        // 判断玩家退出时，房间状态是否处于GAME_START
        if (_statu == GAME_START) // 游戏进行中，玩家A退出，则判断玩家B胜利
        {
            uint64_t winner_id = user_id == _white_id ? _black_id : _white_id;
            std::string reason = user_id == _white_id ? "白棋玩家退出游戏房间，黑棋玩家获胜" : "黑棋玩家退出游戏房间，白棋玩家获胜";
            
            resp["optype"] = "put_chess";
            resp["result"] = true;
            resp["reason"] = reason;
            resp["room_id"] = (Json::UInt64)_room_id;
            resp["uid"] = (Json::UInt64)user_id;
            resp["row"] = -1; // -1 表示玩家掉线，没有走棋
            resp["col"] = -1; // -1 表示玩家掉线，没有走棋
            resp["winner"] = (Json::UInt64)winner_id;

            // 更新数据库中用户信息表的相关信息
            uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
            _user_table->victory(winner_id);
            _user_table->defeat(loser_id);

            _statu = GAME_OVER; // 更新游戏房间的状态

            broadcast(resp); // 将处理信息广播给房间的所有用户
        }

        --_player_count; // 游戏房间中的玩家数量减一
    }

    // 总的请求处理函数，在函数内部区分请求类型，根据不同的请求调用不同的处理函数，将得到的响应进行广播
    void handle_request(const Json::Value& req)
    {
        Json::Value resp;
        
        // 判断req请求中的房间id与当前房间id是否匹配
        uint64_t room_id = req["room_id"].asUInt64();
        if (room_id != _room_id)
        {
            resp["optype"] = req["optype"];
            resp["result"] = false;
            resp["reason"] = "游戏房间id不匹配";
        }
        else
        {
            // 根据req["optype"]来调用不同的处理函数
            if (req["optype"].asString() == "put_chess")
            {
                resp = handle_chess(req);
                if (resp["winner"].asUInt64() != 0) // 说明有玩家获胜
                {
                    // 更新数据库中用户信息表的相关信息
                    uint64_t winner_id = resp["winner"].asUInt64();
                    uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                    _user_table->victory(winner_id);
                    _user_table->defeat(loser_id);

                    // 更新游戏房间的状态
                    _statu = GAME_OVER;
                }
            }
            else if (req["optype"].asString() == "chat")
            {
                resp = handle_chat(req);
            }
            else
            {
                resp["optype"] = req["optype"];
                resp["result"] = false;
                resp["reason"] = "未知类型的请求";
            }
        }

        // 将处理信息广播给房间的所有用户
        broadcast(resp);
    }

    // 将指定的信息广播给房间中所有玩家
    void broadcast(const Json::Value& resp)
    {
        // 1. 对resp进行序列化，将序列化结果保存到一个string中
        std::string resp_str;
        json_util::serialize(resp, resp_str);

        // 2. 获取房间中白棋玩家和黑棋玩家的通信连接，并通过通信连接给玩家发送响应信息
        websocketsvr_t::connection_ptr white_conn = _user_online->get_conn_from_room(_white_id);
        if (white_conn.get() != nullptr) white_conn->send(resp_str);

        websocketsvr_t::connection_ptr black_conn = _user_online->get_conn_from_room(_black_id);
        if (black_conn.get() != nullptr) black_conn->send(resp_str);
    }

private:
    bool five_chess(int row, int col, int row_offset, int col_offset, int chess_color)
    {
        int count = 1; // 将刚刚下的棋也包括在内

        // 判断方向1
        int serch_row = row + row_offset;
        int serch_col = col + col_offset;
        while (serch_row >= 0 && serch_row < BOARD_ROWS && serch_col >= 0 && serch_col <= BOARD_COLS && _board[serch_row][serch_col] == chess_color)
        {
            ++count; // 同色棋子数量++

            // 检索位置继续偏移
            serch_row += row_offset;
            serch_col += col_offset;
        }

        // 判断方向2
        serch_row = row - row_offset;
        serch_col = col - col_offset;
        while (serch_row >= 0 && serch_row < BOARD_ROWS && serch_col >= 0 && serch_col <= BOARD_COLS && _board[serch_row][serch_col] == chess_color)
        {
            ++count; // 同色棋子数量++

            // 检索位置继续偏移
            serch_row -= row_offset;
            serch_col -= col_offset;
        }

        return count >= 5;
    }

    // 返回胜利玩家的id，没有则返回0
    uint64_t check_win(int row, int col, int chess_color)
    {
        // 在下棋的位置检查四个方向是是否有五星连珠的情况(横行，纵列，正斜，反斜)
        if ((five_chess(row, col, 0, 1, chess_color)) || (five_chess(row, col, 1, 0, chess_color)) || (five_chess(row, col, -1, -1, chess_color)) || (five_chess(row, col, -1, 1, chess_color)))
        {
            return chess_color == WHITE_CHESS ? _white_id : _black_id;
        }

        return 0;
    }

    // 敏感词检测
    bool have_sensitive_word(const std::string& msg)
    {
        for (const auto& word : _sensitive_words)
        {
            // 聊天消息中包含敏感词
            if (msg.find(word) != std::string::npos) return true;
        }

        return false;
    }

private:
    uint64_t _room_id;                                // 游戏房间id
    room_statu _statu;                                // 游戏房间的状态
    int _player_count;                                // 游戏房间中玩家的数量
    uint64_t _white_id;                               // 白棋玩家的id
    uint64_t _black_id;                               // 黑棋玩家的id
    user_table* _user_table;                          // 数据库用户信息表的操作句柄
    online_manager* _user_online;                     // 在线用户管理句柄
    std::vector<std::vector<int>> _board;             // 棋盘
    static std::vector<std::string> _sensitive_words; // 聊天敏感词（后期可补充）
};
std::vector<std::string> room::_sensitive_words = {"色情", "裸体", "性爱", "性交", "色情片",
                                         "色情服务", "色情网站", "色情图片", "色情小说",
                                         "操", "滚", "傻逼", "蠢货", "贱人", "混蛋",
                                         "畜生", "白痴", "废物", "黑鬼", "黄种人", "白猪",
                                         "异教徒", "邪教", "基佬", "拉拉", "同性恋", "暴力",
                                         "杀人", "打架", "战斗", "殴打", "刺杀", "爆炸",
                                         "恐怖袭击", "毒品", "赌博", "贩卖", "贿赂", "偷窃",
                                         "抢劫"};

typedef std::shared_ptr<room> room_ptr;
class room_manager
{
public:
    room_manager(user_table* user_table, online_manager* user_online)
        :_next_room_id(1)
        , _user_table(user_table)
        , _user_online(user_online)
    {
        srand(time(nullptr)); // 用于将玩家1和玩家2随机分配给白棋和黑棋
        DLOG("房间管理类初始化完毕");
    }

    ~room_manager() { DLOG("房间管理类即将销毁"); }

    room_ptr create_room(uint64_t user_id1, uint64_t user_id2)
    {
        // 两个玩家都在游戏大厅中匹配成功后才能创建房间
        // 1. 判断玩家1和玩家2是否都在游戏大厅中
        if (_user_online->is_in_game_hall(user_id1) == false) // 玩家1不在游戏大厅中
        {
            DLOG("创建游戏房间失败，玩家：%lu 不在游戏大厅中", user_id1);
            return room_ptr();
        }
        if (_user_online->is_in_game_hall(user_id2) == false) // 玩家2不在游戏大厅中
        {
            DLOG("创建游戏房间失败，玩家：%lu 不在游戏大厅中", user_id2);
            return room_ptr();
        }

        std::unique_lock<std::mutex> lock(_mtx); // 对下面的操作进行加锁保护
        // 2. 创建房间，将用户信息添加到房间中
        room_ptr proom(new room(_next_room_id++, _user_table, _user_online));
        
        // 玩家1和玩家2随机匹配白棋和黑棋
        uint64_t white_id = rand() % 2 == 0 ? user_id1 : user_id2;
        uint64_t black_id = white_id == user_id1 ? user_id2 : user_id1;
        proom->add_white_player(white_id);
        proom->add_black_player(black_id);

        //-----------------------存疑？这里要不要调用_user_online->enter_game_room()？-----------------------

        // 3. 将房间信息管理起来
        _room_id_and_room.insert({proom->id(), proom});
        _user_id_and_room_id.insert({user_id1, proom->id()});
        _user_id_and_room_id.insert({user_id2, proom->id()});

        return proom;
    }

    room_ptr get_room_by_room_id(uint64_t room_id)
    {
        std::unique_lock<std::mutex> lock(_mtx); // 对下面的操作进行加锁保护

        auto it = _room_id_and_room.find(room_id);
        if (it == _room_id_and_room.end()) // 没找到房间号为id的房间
        {
            DLOG("不存在房间id为：%d 的房间", room_id);
            return room_ptr();
        }

        return it->second;
    }

    room_ptr get_room_by_user_id(uint64_t user_id)
    {
        std::unique_lock<std::mutex> lock(_mtx); // 对下面的操作进行加锁保护

        auto it1 = _user_id_and_room_id.find(user_id);
        if (it1 == _user_id_and_room_id.end())
        {
            DLOG("不存在与id为：%d 的玩家匹配的房间");
            return room_ptr();
        }

        uint64_t room_id = it1->second;
        auto it2 = _room_id_and_room.find(room_id);
        if (it2 == _room_id_and_room.end())
        {
            DLOG("不存在房间id为：%d 的房间", room_id);
            return room_ptr();
        }

        return it2->second;
    }

    void remove_player_in_room(uint64_t user_id)
    {
        // 1. 通过玩家id获取游戏房间信息
        room_ptr proom = get_room_by_user_id(user_id);
        if (proom.get() == nullptr)
        {
            DLOG("通过玩家id获取游戏房间信息失败");
            return;
        }

        // 2. 处理玩家退出房间动作
        proom->handle_exit(user_id);

        // 3. 判断游戏房间中是否还有玩家，没有则销毁游戏房间
        if (proom->player_count() == 0) destroy_room(proom->id());
    }

    void destroy_room(uint64_t room_id)
    {
        // 1. 通过房间id获取游戏房间信息
        room_ptr proom = get_room_by_room_id(room_id);
        if (proom.get() == nullptr)
        {
            DLOG("通过房间id获取游戏房间信息失败");
            return;
        }

        // 2. 通过游戏房间获取白棋玩家id和黑棋玩家id
        uint64_t white_id = proom->get_white_player();
        uint64_t black_id = proom->get_black_player();

        {
            std::unique_lock<std::mutex> lock(_mtx); // 加锁保护该作用域中的操作

            // 3. 将白棋玩家和黑棋玩家在“玩家id和游戏房间id的关联关系”中移除
            _user_id_and_room_id.erase(white_id);
            _user_id_and_room_id.erase(black_id);

            // 4. 将游戏房间信息从房间管理中移除
            _room_id_and_room.erase(proom->id());
        }
    }

private:
    uint64_t _next_room_id;                                      // 房间id分配器
    std::mutex _mtx;                                             // 互斥锁
    user_table* _user_table;                                     // 数据库用户信息表的操作句柄
    online_manager* _user_online;                                // 在线用户管理句柄
    std::unordered_map<uint64_t, room_ptr> _room_id_and_room;    // 游戏房间id和游戏房间的关联关系
    std::unordered_map<uint64_t, uint64_t> _user_id_and_room_id; // 玩家id和游戏房间id的关联关系
};