#pragma once

#include <QObject>
#include <QSerialPort>

class RegisterStore;

class ModbusRtuSlave : public QObject
{
    Q_OBJECT

public:
    enum class ExceptionCode : quint8 {
        IllegalFunction = 0x01,
        IllegalDataAddress = 0x02,
        IllegalDataValue = 0x03
    };

    explicit ModbusRtuSlave(RegisterStore *store, QObject *parent = nullptr);
    ~ModbusRtuSlave() override;

    bool openPort(const QString &portName,
                  qint32 baudRate,
                  QSerialPort::DataBits dataBits,
                  QSerialPort::Parity parity,
                  QSerialPort::StopBits stopBits,
                  quint8 slaveId,
                  QString *errorMessage = nullptr);
    void closePort();
    bool isOpen() const;
    quint8 slaveId() const;

    static quint16 crc16(const QByteArray &data);
    static QString toHex(const QByteArray &data);

signals:
    void logMessage(const QString &message);
    void statusChanged(const QString &message, bool opened);
    void frameReceived(const QByteArray &frame);
    void frameSent(const QByteArray &frame);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    void processBuffer();
    QByteArray handleRequest(const QByteArray &request);
    QByteArray buildExceptionResponse(quint8 functionCode, ExceptionCode code) const;
    QByteArray buildNormalResponse(quint8 functionCode, const QByteArray &payload) const;

    QSerialPort m_serialPort;
    RegisterStore *m_store = nullptr;
    QByteArray m_buffer;
    quint8 m_slaveId = 1;
};
