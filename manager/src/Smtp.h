#pragma once

#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QSslSocket>
#include <QString>
#include <QTextStream>
#include <QDebug>
#include <QtWidgets/QMessageBox>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>


class Smtp : public QObject
{
    Q_OBJECT


public:
    Smtp(const QString& user, const QString& pass, const QString& host, int port = 465, int timeout = 30000);
    ~Smtp();

    void sendTextMail(const QString& from, const QString& to, const QString& subject, const QString& body, const QStringList& files = QStringList());
    void sendHtmlMail(const QString& from, const QString& to, const QString& subject, const QString& body, const QStringList& files = QStringList());

signals:
    void status(const QString&);

private slots:
    void stateChanged(QAbstractSocket::SocketState socketState);
    void errorReceived(QAbstractSocket::SocketError socketError);
    void disconnected();
    void connected();
    void readyRead();

private:
    void _sendMail(const QString& from, const QString& to, const QString& subject, const QString& body, bool html, const QStringList& files);

    int m_timeout = 0;
    QString m_message;
    QTextStream *m_stream = nullptr;
    QSslSocket *m_socket;
    QString m_from;
    QString m_rcpt;
    QString m_response;
    QString m_user;
    QString m_pass;
    QString m_host;
    int m_port = 0;

    enum class State
    {
        Tls,
        HandShake,
        Auth,
        User,
        Pass,
        Rcpt,
        Mail,
        Data,
        Init,
        Body,
        Quit,
        Close
    };
    State m_state = State::Tls;
};
