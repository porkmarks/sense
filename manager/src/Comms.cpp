#include "Comms.h"

Comms::Comms()
{
    m_udpSocket.reset(new QUdpSocket());
    m_udpSocket->bind(5555, QUdpSocket::ShareAddress);

    QObject::connect(m_udpSocket.get(), &QUdpSocket::readyRead, [this]() { broadcastReceived(); });
}

void Comms::broadcastReceived()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        if (m_udpSocket->pendingDatagramSize() == 6)
        {
            std::array<uint8_t, 6> mac;
            QHostAddress address;
            uint16_t port = 0;
            if (m_udpSocket->readDatagram(reinterpret_cast<char*>(mac.data()), mac.size(), &address, &port) == 6)
            {
                emit baseStationDiscovered(mac, address, port);
            }
        }
        else
        {
            m_udpSocket->readDatagram(nullptr, 0);
        }
    }
}
