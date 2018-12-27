#include "Crypt.h"
#include <QByteArray>
#include <QtDebug>
#include <QtGlobal>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDataStream>

Crypt::Crypt():
    m_key(0),
    m_compressionMode(CompressionAuto),
    m_protectionMode(ProtectionChecksum),
    m_lastError(ErrorNoError)
{
    qsrand(uint(QDateTime::currentMSecsSinceEpoch() & 0xFFFF));
}

Crypt::Crypt(quint64 key):
    m_key(key),
    m_compressionMode(CompressionAuto),
    m_protectionMode(ProtectionChecksum),
    m_lastError(ErrorNoError)
{
    qsrand(uint(QDateTime::currentMSecsSinceEpoch() & 0xFFFF));
    splitKey();
}

void Crypt::setKey(const QString& key)
{
    uint64_t k = 123423461235342ULL;
    for (int i = 0; i < key.size(); i++)
    {
        char c = key.at(i).toLatin1();
        k ^= c + 0x9e3779b9ULL + (k<<6) + (k>>2);
    }
    setKey(k);
}

void Crypt::setKey(quint64 key)
{
    m_key = key;
    splitKey();
}

void Crypt::splitKey()
{
    m_keyParts.clear();
    m_keyParts.resize(8);
    for (int i=0;i<8;i++) {
        quint64 part = m_key;
        for (int j=i; j>0; j--)
            part = part >> 8;
        part = part & 0xff;
        m_keyParts[i] = static_cast<char>(part);
    }
}

void Crypt::setAddRandomSalt(bool salt)
{
    m_addRandomSalt = salt;
}

QByteArray Crypt::encryptToByteArray(const QString& plaintext)
{
    QByteArray plaintextArray = plaintext.toUtf8();
    return encryptToByteArray(plaintextArray);
}

QByteArray Crypt::encryptToByteArray(QByteArray plaintext)
{
    if (m_keyParts.isEmpty()) {
        qWarning() << "No key set.";
        m_lastError = ErrorNoKeySet;
        return QByteArray();
    }


    QByteArray ba = plaintext;

    CryptoFlags flags = CryptoFlagNone;
    if (m_compressionMode == CompressionAlways) {
        ba = qCompress(ba, m_compressionLevel); //maximum compression
        flags |= CryptoFlagCompression;
    } else if (m_compressionMode == CompressionAuto) {
        QByteArray compressed = qCompress(ba, m_compressionLevel);
        if (compressed.count() < ba.count()) {
            ba = compressed;
            flags |= CryptoFlagCompression;
        }
    }

    QByteArray integrityProtection;
    if (m_protectionMode == ProtectionChecksum) {
        flags |= CryptoFlagChecksum;
        QDataStream s(&integrityProtection, QIODevice::WriteOnly);
        s << qChecksum(ba.constData(), ba.length());
    } else if (m_protectionMode == ProtectionHash) {
        flags |= CryptoFlagHash;
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(ba);

        integrityProtection += hash.result();
    }

    //prepend a random char to the string
    char randomChar = m_addRandomSalt ? char(qrand() & 0xFF) : 0;
    ba = randomChar + integrityProtection + ba;

    int pos(0);
    char lastChar(0);

    int cnt = ba.count();

    {
        char* data = ba.data();
        while (pos < cnt) {
            char d = data[pos] ^ m_keyParts.at(pos % 8) ^ lastChar;
            data[pos] = d;
            lastChar = d;
            ++pos;
        }
    }

    QByteArray resultArray;
    resultArray.append(char(0x03));  //version for future updates to algorithm
    resultArray.append(char(flags)); //encryption flags
    resultArray.append(ba);

    m_lastError = ErrorNoError;
    return resultArray;
}

QString Crypt::encryptToString(const QString& plaintext)
{
    QByteArray plaintextArray = plaintext.toUtf8();
    QByteArray cypher = encryptToByteArray(plaintextArray);
    QString cypherString = QString::fromLatin1(cypher.toBase64());
    return cypherString;
}

QString Crypt::encryptToString(QByteArray plaintext)
{
    QByteArray cypher = encryptToByteArray(plaintext);
    QString cypherString = QString::fromLatin1(cypher.toBase64());
    return cypherString;
}

QString Crypt::decryptToString(const QString &cyphertext)
{
    QByteArray cyphertextArray = QByteArray::fromBase64(cyphertext.toLatin1());
    QByteArray plaintextArray = decryptToByteArray(cyphertextArray);
    QString plaintext = QString::fromUtf8(plaintextArray, plaintextArray.size());

    return plaintext;
}

QString Crypt::decryptToString(QByteArray cypher)
{
    QByteArray ba = decryptToByteArray(cypher);
    QString plaintext = QString::fromUtf8(ba, ba.size());

    return plaintext;
}

QByteArray Crypt::decryptToByteArray(const QString& cyphertext)
{
    QByteArray cyphertextArray = QByteArray::fromBase64(cyphertext.toLatin1());
    QByteArray ba = decryptToByteArray(cyphertextArray);

    return ba;
}

QByteArray Crypt::decryptToByteArray(QByteArray cypher)
{
    if (m_keyParts.isEmpty()) {
        qWarning() << "No key set.";
        m_lastError = ErrorNoKeySet;
        return QByteArray();
    }

    QByteArray ba = cypher;

    if( cypher.count() < 3 )
        return QByteArray();

    char version = ba.at(0);

    if (version !=3) {  //we only work with version 3
        m_lastError = ErrorUnknownVersion;
        qWarning() << "Invalid version or not a cyphertext.";
        return QByteArray();
    }

    CryptoFlags flags = CryptoFlags(ba.at(1));

    ba = ba.mid(2);
    int pos(0);
    int cnt(ba.count());
    char lastChar = 0;

    {
        char* data = ba.data();
        while (pos < cnt) {
            char currentChar = data[pos];
            data[pos] = currentChar ^ lastChar ^ m_keyParts.at(pos % 8);
            lastChar = currentChar;
            ++pos;
        }
    }

    ba = ba.mid(1); //chop off the random number at the start

    bool integrityOk(true);
    if (flags.testFlag(CryptoFlagChecksum)) {
        if (ba.length() < 2) {
            m_lastError = ErrorIntegrityFailed;
            return QByteArray();
        }
        quint16 storedChecksum;
        {
            QDataStream s(&ba, QIODevice::ReadOnly);
            s >> storedChecksum;
        }
        ba = ba.mid(2);
        quint16 checksum = qChecksum(ba.constData(), ba.length());
        integrityOk = (checksum == storedChecksum);
    } else if (flags.testFlag(CryptoFlagHash)) {
        if (ba.length() < 20) {
            m_lastError = ErrorIntegrityFailed;
            return QByteArray();
        }
        QByteArray storedHash = ba.left(20);
        ba = ba.mid(20);
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(ba);
        integrityOk = (hash.result() == storedHash);
    }

    if (!integrityOk) {
        m_lastError = ErrorIntegrityFailed;
        return QByteArray();
    }

    if (flags.testFlag(CryptoFlagCompression))
        ba = qUncompress(ba);

    m_lastError = ErrorNoError;
    return ba;
}
