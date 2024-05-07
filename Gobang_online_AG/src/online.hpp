#pragma once

#include <unordered_map>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <mutex>

#include "logger.hpp"

typedef websocketpp::server<websocketpp::config::asio> websocketsvr_t;

class online_manager
{
public:
    // websocket连接建立的时候，才会加入游戏大厅的在线用户管理
    void enter_game_hall(uint64_t user_id, websocketsvr_t::connection_ptr& conn)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _hall_users.insert({user_id, conn});
    }

    // websocket连接建立的时候，才会加入游戏房间的在线用户管理
    bool enter_game_room(uint64_t user_id, websocketsvr_t::connection_ptr& conn)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _room_users.insert({user_id, conn});
    }

    // websocket连接断开的时候，才会移除游戏大厅的在线用户管理
    bool exit_game_hall(uint64_t user_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _hall_users.erase(user_id);
    }

    // websocket连接断开的时候，才会移除游戏房间的在线用户管理
    bool exit_game_room(uint64_t user_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _room_users.erase(user_id);
    }

    // 判断指定用户当前是否在游戏大厅中
    bool is_in_game_hall(uint64_t user_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        if (_hall_users.find(user_id) == _hall_users.end()) return false;
        return true;
    }

    // 判断指定用户当前是否在游戏房间中
    bool is_in_game_room(uint64_t user_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        if (_room_users.find(user_id) == _room_users.end()) return false;
        return true;
    }

    // 通过用户id在游戏大厅用户管理中获取对应用户的通信连接
    websocketsvr_t::connection_ptr get_conn_from_hall(uint64_t user_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _hall_users.find(user_id);
        if (it == _hall_users.end()) return websocketsvr_t::connection_ptr();
        return it->second;
    }

    // 通过用户id在游戏房间用户管理中获取对应用户的通信连接
    websocketsvr_t::connection_ptr get_conn_from_room(uint64_t user_id)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        auto it = _room_users.find(user_id);
        if (it == _room_users.end()) return websocketsvr_t::connection_ptr();
        return it->second;
    }

private:
    std::unordered_map<uint64_t, websocketsvr_t::connection_ptr> _hall_users; // 用于建立在游戏大厅的用户的用户id与通信连接的关系
    std::unordered_map<uint64_t, websocketsvr_t::connection_ptr> _room_users; // 用于建立在游戏房间的用户的用户id与通信连接的关系
    std::mutex _mtx;
};