#pragma once

#include "db.hpp"
#include "online.hpp"
#include "util.hpp"

#include <vector>

#define BOARD_ROWS 15
#define BOARD_COLS 15
#define CHESS_WHITE 1
#define CHESS_BLACK 2

enum room_status
{
    GAME_START,
    GAME_OVER
};

class room
{
public:
    room(uint64_t room_id, user_table* user_tb, online_manager* user_ol)
        :_room_id(room_id)
        , _status(GAME_START)
        , _player_count(0)
        , _user_tb(user_tb)
        , _user_ol(user_ol)
        , _board(BOARD_ROWS, std::vector<int>(BOARD_COLS, 0))
    {
        DLOG("%lu 房间创建成功", _room_id);
    }

    uint64_t id() { return _room_id; }

    room_status status() { return _status; }

    int player_count() { return _player_count; }

    void add_white_player(uint64_t userId)
    {
        _white_id = userId;
        ++_player_count;
    }

    void add_black_player(uint64_t userId)
    {
        _white_id = userId;
        ++_player_count;
    }

    uint64_t get_white_player() { return _white_id; }

    uint64_t get_black_player() { return _black_id; }

    // 处理下棋动作
    Json::Value handle_chess(Json::Value& req)
    {
        Json::Value json_resp;

        uint64_t cur_userId = req["uid"].asUInt64();
        int chess_row = req["row"].asInt();
        int chess_col = req["col"].asInt();

        // 1.判断走棋位置是否合理(是否越界，是否被占用)
        if (chess_row > BOARD_ROWS || chess_col > BOARD_COLS)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = false;
            json_resp["reason"] = "chess position out of bounds";

            return json_resp;
        }
        else if (_board[chess_row][chess_col] != 0)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = false;
            json_resp["reason"] = "the current location is occupied";

            return json_resp;
        }

        // 下棋
        int cur_col = cur_userId == _white_id ? CHESS_WHITE : CHESS_BLACK;
        _board[chess_row][chess_col] = cur_col;

        // 2.判断房间中两个玩家是否在线，若有一个退出，则判另一个获胜
        // 判断白棋玩家是否在线
        if (_user_ol->is_in_game_room(_white_id) == false) // 白棋玩家掉线
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "the other player drops out, wins without a fight";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = (Json::UInt64)cur_userId;
            json_resp["row"] = chess_row;
            json_resp["col"] = chess_col;
            json_resp["winner"] = (Json::UInt64)_black_id;

            return json_resp;
        }
        // 判断黑棋玩家是否在线
        if (_user_ol->is_in_game_room(_black_id) == false) // 黑棋玩家掉线
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "the other player drops out, wins without a fight";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = (Json::UInt64)cur_userId;
            json_resp["row"] = chess_row;
            json_resp["col"] = chess_col;
            json_resp["winner"] = (Json::UInt64)_white_id;

            return json_resp;
        }

        // 3.判断是否有玩家胜利(从当前走棋位置开始判断，是否存在五星连珠)
        uint64_t winner_id = check_win(chess_row, chess_col, cur_col); // winner_id = 0表示没有玩家获胜
        if (winner_id != 0)
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "five chess on one line, game over";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = (Json::UInt64)cur_userId;
            json_resp["row"] = chess_row;
            json_resp["col"] = chess_col;
            json_resp["winner"] = (Json::UInt64)winner_id;

            return json_resp;
        }
        
        // 正常走棋
        json_resp["optype"] = "put_chess";
        json_resp["result"] = true;
        json_resp["reason"] = "normal moves, game continues";
        json_resp["room_id"] = (Json::UInt64)_room_id;
        json_resp["uid"] = (Json::UInt64)cur_userId;
        json_resp["row"] = chess_row;
        json_resp["col"] = chess_col;
        json_resp["winner"] = (Json::UInt64)winner_id;

        return json_resp;
    }

    // 处理聊天动作
    Json::Value handle_chat(Json::Value& req)
    {
        Json::Value json_resp;

        // 1.检测聊天信息中是否包含敏感词
        std::string msg = json_resp["message"].asString();
        if (msg.find("垃圾") != std::string::npos) // 聊天信息中包含敏感词 ----- 后续可以完善一个敏感词检测接口
        {
            json_resp["optype"] = "chat";
            json_resp["result"] = false;
            json_resp["reason"] = "message contains sensitive words";

            return json_resp;
        }

        // 2.返回
        json_resp = req;
        json_resp["result"] = true;

        return json_resp;
    }

    // 处理玩家退出游戏房间动作
    void handle_exit(uint64_t userId)
    {
        // 若是在下棋过程中退出，则对方胜利；游戏结束后玩家退出，是正常退出
        Json::Value json_resp;
        if (_status == GAME_START) // 游戏中，userId玩家退出
        {
            json_resp["optype"] = "put_chess";
            json_resp["result"] = true;
            json_resp["reason"] = "the other player drops out, wins without a fight";
            json_resp["room_id"] = (Json::UInt64)_room_id;
            json_resp["uid"] = (Json::UInt64)userId;
            json_resp["row"] = -1; // 玩家掉线，没有走棋
            json_resp["col"] = -1; // 玩家掉线，没有走棋
            json_resp["winner"] = (Json::UInt64)(userId == _white_id ? _black_id : _white_id);

            // TODO-更新数据库

            broadcast(json_resp); // 广播给房间的其他用户
        }

        // 游戏房间中玩家数量--
        --_player_count;
    }

    // 总的请求处理函数，在函数内部区分请求类型，根据不同的请求调用不同的处理函数，将得到的响应进行广播
    void handle_request(Json::Value& req)
    {
        Json::Value json_resp;
        // 1.判断请求中的房间id与当前房间id是否匹配
        uint64_t room_id = req["room_id"].asUInt64();
        if (room_id != _room_id)
        {
            json_resp["optype"] = req["optype"];
            json_resp["result"] = false;
            json_resp["reason"] = "room id doesn't match";
        }
        else
        {
            // 2.根据不同的请求调用不同的处理函数
            if (json_resp["optype"] == "put_chess")
            {
                json_resp = handle_chess(req);

                if (json_resp["winner"].asUInt64() != 0) // 说明有玩家获胜
                {
                    // 更新数据库中相关用户的信息
                    uint64_t winner_id = json_resp["winner"].asUInt64();
                    uint64_t loser_id = winner_id == _white_id ? _black_id : _white_id;
                    _user_tb->victory(winner_id);
                    _user_tb->defeat(loser_id);

                    // 更新游戏房间的状态
                    _status = GAME_OVER;
                }
            }
            else if (json_resp["optype"] == "chat")
            {
                json_resp = handle_chat(req);
            }
            else
            {
                json_resp["optype"] = req["optype"];
                json_resp["result"] = false;
                json_resp["reason"] = "unknown optype";
            }
        }

        // 3.广播
        broadcast(json_resp);
    }

    // 将指定的信息广播给房间中所有玩家
    void broadcast(Json::Value& resp)
    {
        // 1.对响应信息进行序列化，将Json::Value中的数据序列化为json格式的字符串存到一个string中
        std::string body;
        json_util::serialize(resp, body);

        // 2.获取房间中所有用户的通信连接
        // 3.给所有用户发送响应信息
        wsserver_t::connection_ptr wconn= _user_ol->get_conn_from_room(_white_id);
        if (wconn.get() != nullptr)
        {
            wconn->send(body);
        }
        wsserver_t::connection_ptr bconn= _user_ol->get_conn_from_room(_black_id);
        if (bconn.get() != nullptr)
        {
            bconn->send(body);
        }
    }

    ~room()
    {
        DLOG("%lu 房间销毁成功", _room_id);
    }

private:
    bool five(int row, int col, int row_offset, int col_offset, int color)
    {
        int count = 1; // 将刚下的棋计入在内

        // 检索方向1
        int search_row = row + row_offset;
        int search_col = col + col_offset;
        while (search_row <= BOARD_ROWS && search_col <= BOARD_COLS && _board[search_row][search_col] == color)
        {
            // 同色棋子数量++
            ++count;

            // 检索位置继续偏移
            search_row += row_offset;
            search_col += col_offset;
        }

        // 检索方向2
        search_row = row - row_offset;
        search_col = col - col_offset;
        while (search_row <= BOARD_ROWS && search_col <= BOARD_COLS && _board[search_row][search_col] == color)
        {
            // 同色棋子数量++
            ++count;

            // 检索位置继续偏移
            search_row -= row_offset;
            search_col -= col_offset;
        }

        return count >= 5;
    }

    // 返回胜利玩家的id，没有则返回0
    uint64_t check_win(int row, int col, int color)
    {
        // 在下棋的位置检查四个方向是是否有五星连珠的情况(横行，纵列，正斜，反斜)
        if (five(row, col, 0, 1, color) || five(row, col, 1, 0, color) || five(row, col, -1, 1, color) || five(row, col, 1, -1, color))
        {
            return color == CHESS_WHITE ? _white_id : _black_id;
        }

        return 0;
    }

private:
    uint64_t _room_id; // 游戏房间id
    room_status _status; // 游戏房间的状态
    int _player_count; // 游戏房间中玩家的数量
    uint64_t _white_id; // 白棋玩家id
    uint64_t _black_id; // 黑棋玩家id
    user_table* _user_tb; // 用户信息表操作句柄
    online_manager* _user_ol; // 在线用户管理句柄
    std::vector<std::vector<int>> _board; // 棋盘
};