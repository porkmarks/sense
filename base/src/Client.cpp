#include "Client.h"
#include <iostream>


Client::Client(boost::asio::io_service& io_service)
    : m_io_service(io_service)
    , m_socket(m_io_service)
{
}

bool Client::init(boost::asio::ip::tcp::endpoint server)
{
    try
    {
        boost::system::error_code ec;

        m_socket.open(boost::asio::ip::tcp::v4(), ec);
        if (ec)
        {
            std::cerr << "Error while openning socket: " << ec.message() << "\n";
            return false;
        }

        m_socket.set_option(boost::asio::ip::tcp::socket::reuse_address(true), ec);
        if (ec)
        {
            std::cerr << "Error while setting option: " << ec.message() << "\n";
            return false;
        }

        m_socket.connect(server, ec);
        if (ec)
        {
            m_socket.close();
            std::cerr << "Error while connecting: " << ec.message() << "\n";
            return false;
        }
        return true;
    }
    catch (std::exception e)
    {
        std::cerr << "Exception while connecting: " << e.what() << "\n";
        m_socket.close();
        return false;
    }
}
