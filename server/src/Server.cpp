#include "Server.h"
#include <iostream>
#include <boost/bind.hpp>

Server::Server(boost::asio::io_service& io_service)
    : m_io_service(io_service)
    , m_acceptor(m_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 50000))
{
    m_acceptor.set_option(boost::asio::ip::tcp::socket::reuse_address(true));
}

void Server::start()
{
    std::shared_ptr<Client> client = std::make_shared<Client>(m_io_service);
    m_acceptor.async_accept(client->get_socket(), boost::bind(&Server::handle_accept, this, client, boost::asio::placeholders::error));
}

void Server::handle_accept(std::shared_ptr<Client> client, const boost::system::error_code& error)
{
    if (!error)
    {
        m_clients.push_back(client);
        std::cout << "New client: " << m_clients.size() << "\n";

        //client->start();
        start();
    }
}

void Server::process()
{

}
