#pragma once

#include <boost/asio.hpp>


class Client
{
public:
    Client(boost::asio::io_service& io_service);

    bool init(boost::asio::ip::tcp::endpoint server);

    void process();

private:
    boost::asio::io_service& m_io_service;
    boost::asio::ip::tcp::socket m_socket;
};
