#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include "Client.h"


class Server
{
public:
    Server(boost::asio::io_service& io_service);

    void start();

    void process();

private:
    void handle_accept(std::shared_ptr<Client> client, const boost::system::error_code& error);

    boost::asio::io_service& m_io_service;
    boost::asio::ip::tcp::acceptor m_acceptor;

    std::vector<std::shared_ptr<Client>> m_clients;
};
