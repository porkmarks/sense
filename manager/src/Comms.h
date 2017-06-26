#pragma once

#include <memory>
#include <QUdpSocket>

class Comms : public QObject
{
    Q_OBJECT

public:
    Comms();

signals:
    void baseStationDiscovered(std::array<uint8_t, 6> const& mac, QHostAddress const& address, uint16_t port);

private:
    void broadcastReceived();

    std::unique_ptr<QUdpSocket> m_udpSocket;
};

