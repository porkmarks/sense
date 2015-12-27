#include "Client.h"
#include <iostream>


Client::Client(boost::asio::io_service& io_service)
    : m_io_service(io_service)
    , m_socket(m_io_service)
{

}

boost::asio::ip::tcp::socket& Client::get_socket()
{
    return m_socket;
}
