#include "Smtp.h"

Smtp::Smtp(const QString& user, const QString& pass, const QString& host, int port, int timeout)
{
    m_socket = new QSslSocket(this);

    connect(m_socket, SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(m_socket, SIGNAL(connected()), this, SLOT(connected()));
    connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this,SLOT(errorReceived(QAbstractSocket::SocketError)));
    connect(m_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(stateChanged(QAbstractSocket::SocketState)));
    connect(m_socket, SIGNAL(disconnected()), this,SLOT(disconnected()));

    m_user = user;
    m_pass = pass;

    m_host = host;
    m_port = port;
    m_timeout = timeout;
}

void Smtp::sendTextMail(const QString& from, const QString& to, const QString& subject, const QString& body, const QStringList& files)
{
    _sendMail(from, to, subject, body, false, files);
}

void Smtp::sendHtmlMail(const QString& from, const QString& to, const QString& subject, const QString& body, const QStringList& files)
{
    _sendMail(from, to, subject, body, true, files);
}

void Smtp::_sendMail(const QString& from, const QString& to, const QString& subject, const QString& body, bool html, const QStringList& files)
{
    m_message = "To: " + to + "\n";
    m_message.append("From: " + from + "\n");
    m_message.append("Subject: " + subject + "\n");

    //Let's intitiate multipart MIME with cutting boundary "frontier"
    m_message.append("MIME-Version: 1.0\n");
    m_message.append("Content-Type: multipart/mixed; boundary=frontier\n\n");

    m_message.append("--frontier\n");
    if (html)
    {
        m_message.append("Content-Type: text/html\n\n");
    }
    else
    {
        m_message.append("Content-Type: text/plain\n\n");
    }
    m_message.append(body);
    m_message.append("\n\n");

    if(!files.isEmpty())
    {
        qDebug() << "Files to be sent: " << files.size();
        foreach(QString filePath, files)
        {
            QFile file(filePath);
            if(file.exists())
            {
                if (!file.open(QIODevice::ReadOnly))
                {
                    qDebug("Couldn't open the file");
                    QMessageBox::warning(0, tr("Qt Simple SMTP client"), tr("Couldn't open the file\n\n") );
                    return ;
                }
                QByteArray bytes = file.readAll();
                m_message.append("--frontier\n");
                m_message.append("Content-Type: application/octet-stream\nContent-Disposition: attachment; filename="+ QFileInfo(file.fileName()).fileName() +";\nContent-Transfer-Encoding: base64\n\n");
                m_message.append(bytes.toBase64());
                m_message.append("\n");
            }
        }
    }
    else
    {
        qDebug() << "No attachments found";
    }

    m_message.append("--frontier--\n");

    m_message.replace(QString::fromLatin1("\n"), QString::fromLatin1("\r\n"));
    m_message.replace(QString::fromLatin1("\r\n.\r\n"),QString::fromLatin1("\r\n..\r\n"));

    m_from = from;
    m_rcpt = to;
    m_state = State::Init;
    m_socket->connectToHostEncrypted(m_host, m_port); //"smtp.gmail.com" and 465 for gmail TLS
    if (!m_socket->waitForConnected(m_timeout))
    {
        qDebug() << m_socket->errorString();
    }

    m_stream = new QTextStream(m_socket);
}

Smtp::~Smtp()
{
    delete m_stream;
    delete m_socket;
}
void Smtp::stateChanged(QAbstractSocket::SocketState socketState)
{

    qDebug() <<"stateChanged " << socketState;
}

void Smtp::errorReceived(QAbstractSocket::SocketError socketError)
{
    qDebug() << "error " <<socketError;
}

void Smtp::disconnected()
{
    qDebug() <<"disconneted";
    qDebug() << "error "  << m_socket->errorString();
}

void Smtp::connected()
{
    qDebug() << "Connected ";
}

void Smtp::readyRead()
{
    qDebug() <<"readyRead";
    // SMTP is line-oriented

    QString responseLine;
    do
    {
        responseLine = m_socket->readLine();
        m_response += responseLine;
    }
    while (m_socket->canReadLine() && responseLine[3] != ' ');

    responseLine.truncate(3);

    qDebug() << "Server response code:" <<  responseLine;
    qDebug() << "Server response: " << m_response;

    if (m_state == State::Init && responseLine == "220")
    {
        // banner was okay, let's go on
        *m_stream << "EHLO localhost" <<"\r\n";
        m_stream->flush();

        m_state = State::HandShake;
    }
    //No need, because I'm using socket->startClienEncryption() which makes the SSL handshake for you
    /*else if (state == State::Tls && responseLine == "250")
    {
        // Trying AUTH
        qDebug() << "STarting Tls";
        *t << "STARTTLS" << "\r\n";
        t->flush();
        state = State::HandShake;
    }*/
    else if (m_state == State::HandShake && responseLine == "250")
    {
        m_socket->startClientEncryption();
        if(!m_socket->waitForEncrypted(m_timeout))
        {
            qDebug() << m_socket->errorString();
            m_state = State::Close;
        }


        //Send EHLO once again but now encrypted

        *m_stream << "EHLO localhost" << "\r\n";
        m_stream->flush();
        m_state = State::Auth;
    }
    else if (m_state == State::Auth && responseLine == "250")
    {
        // Trying AUTH
        qDebug() << "Auth";
        *m_stream << "AUTH LOGIN" << "\r\n";
        m_stream->flush();
        m_state = State::User;
    }
    else if (m_state == State::User && responseLine == "334")
    {
        //Trying User
        qDebug() << "Username";
        //GMAIL is using XOAUTH2 protocol, which basically means that password and username has to be sent in base64 coding
        //https://developers.google.com/gmail/xoauth2_protocol
        *m_stream << QByteArray().append(m_user).toBase64()  << "\r\n";
        m_stream->flush();

        m_state = State::Pass;
    }
    else if (m_state == State::Pass && responseLine == "334")
    {
        //Trying pass
        qDebug() << "Pass";
        *m_stream << QByteArray().append(m_pass).toBase64() << "\r\n";
        m_stream->flush();

        m_state = State::Mail;
    }
    else if (m_state == State::Mail && responseLine == "235")
    {
        // HELO response was okay (well, it has to be)

        //Apperantly for Google it is mandatory to have MAIL FROM and RCPT email formated the following way -> <email@gmail.com>
        qDebug() << "MAIL FROM:<" << m_from << ">";
        *m_stream << "MAIL FROM:<" << m_from << ">\r\n";
        m_stream->flush();
        m_state = State::Rcpt;
    }
    else if (m_state == State::Rcpt && responseLine == "250")
    {
        //Apperantly for Google it is mandatory to have MAIL FROM and RCPT email formated the following way -> <email@gmail.com>
        *m_stream << "RCPT TO:<" << m_rcpt << ">\r\n"; //r
        m_stream->flush();
        m_state = State::Data;
    }
    else if (m_state == State::Data && responseLine == "250")
    {

        *m_stream << "DATA\r\n";
        m_stream->flush();
        m_state = State::Body;
    }
    else if (m_state == State::Body && responseLine == "354")
    {

        *m_stream << m_message << "\r\n.\r\n";
        m_stream->flush();
        m_state = State::Quit;
    }
    else if (m_state == State::Quit && responseLine == "250")
    {

        *m_stream << "QUIT\r\n";
        m_stream->flush();
        // here, we just close.
        m_state = State::Close;
        emit status(tr("Message sent"));
    }
    else if (m_state == State::Close)
    {
        deleteLater();
        return;
    }
    else
    {
        // something broke.
        QMessageBox::warning(0, tr("Qt Simple SMTP client"), tr("Unexpected reply from SMTP server:\n\n") + m_response);
        m_state = State::Close;
        emit status(tr("Failed to send message"));
    }
    m_response = "";
}
