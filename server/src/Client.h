#pragma once

#include <boost/asio.hpp>


class Client
{
public:
    Client(boost::asio::io_service& io_service);

    boost::asio::ip::tcp::socket& get_socket();

    void process();

private:
    boost::asio::io_service& m_io_service;
    boost::asio::ip::tcp::socket m_socket;
};
