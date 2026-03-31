#include "ModbusRtuSlave.h"

#include "RegisterStore.h"

#include <QDataStream>

namespace {
constexpr int kMinimumFrameSize = 8;
}

ModbusRtuSlave::ModbusRtuSlave(RegisterStore *store, QObject *parent)
    : QObject(parent),
      m_store(store)
{
    connect(&m_serialPort, &QSerialPort::readyRead, this, &ModbusRtuSlave::onReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred, this, &ModbusRtuSlave::onErrorOccurred);
}

ModbusRtuSlave::~ModbusRtuSlave()
{
    closePort();
}

bool ModbusRtuSlave::openPort(const QString &portName,
                              qint32 baudRate,
                              QSerialPort::DataBits dataBits,
                              QSerialPort::Parity parity,
                              QSerialPort::StopBits stopBits,
                              quint8 slaveId,
                              QString *errorMessage)
{
    closePort();

    m_serialPort.setPortName(portName);
    m_serialPort.setBaudRate(baudRate);
    m_serialPort.setDataBits(dataBits);
    m_serialPort.setParity(parity);
    m_serialPort.setStopBits(stopBits);
    m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort.open(QIODevice::ReadWrite)) {
        if (errorMessage) {
            *errorMessage = m_serialPort.errorString();
        }
        emit statusChanged(QStringLiteral("串口打开失败"), false);
        return false;
    }

    m_slaveId = slaveId;
    m_buffer.clear();
    emit statusChanged(QStringLiteral("串口已打开"), true);
    emit logMessage(QStringLiteral("串口 %1 已打开，Slave ID=%2").arg(portName).arg(slaveId));
    return true;
}

void ModbusRtuSlave::closePort()
{
    if (m_serialPort.isOpen()) {
        const QString portName = m_serialPort.portName();
        m_serialPort.close();
        emit statusChanged(QStringLiteral("串口已关闭"), false);
        emit logMessage(QStringLiteral("串口 %1 已关闭").arg(portName));
    }
    m_buffer.clear();
}

bool ModbusRtuSlave::isOpen() const
{
    return m_serialPort.isOpen();
}

quint8 ModbusRtuSlave::slaveId() const
{
    return m_slaveId;
}

quint16 ModbusRtuSlave::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

QString ModbusRtuSlave::toHex(const QByteArray &data)
{
    QStringList parts;
    for (const auto byte : data) {
        parts.append(QStringLiteral("%1").arg(static_cast<quint8>(byte), 2, 16, QLatin1Char('0')).toUpper());
    }
    return parts.join(' ');
}

void ModbusRtuSlave::onReadyRead()
{
    m_buffer.append(m_serialPort.readAll());
    processBuffer();
}

void ModbusRtuSlave::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    emit logMessage(QStringLiteral("串口错误：%1").arg(m_serialPort.errorString()));
}

void ModbusRtuSlave::processBuffer()
{
    // 简化实现：当前按“一次收到一个完整请求帧”处理，满足题目允许条件。
    while (m_buffer.size() >= kMinimumFrameSize) {
        const QByteArray frame = m_buffer;
        m_buffer.clear();
        emit frameReceived(frame);
        emit logMessage(QStringLiteral("RX: %1").arg(toHex(frame)));

        const QByteArray response = handleRequest(frame);
        if (!response.isEmpty() && m_serialPort.isOpen()) {
            m_serialPort.write(response);
            emit frameSent(response);
            emit logMessage(QStringLiteral("TX: %1").arg(toHex(response)));
        }
    }
}

QByteArray ModbusRtuSlave::handleRequest(const QByteArray &request)
{
    if (request.size() < kMinimumFrameSize) {
        emit logMessage(QStringLiteral("收到帧长度不足，已忽略。"));
        return {};
    }

    const quint8 requestSlaveId = static_cast<quint8>(request.at(0));
    const quint8 functionCode = static_cast<quint8>(request.at(1));

    if (requestSlaveId != m_slaveId) {
        emit logMessage(QStringLiteral("收到非本机地址 %1 的帧，已忽略。").arg(requestSlaveId));
        return {};
    }

    const QByteArray payloadWithoutCrc = request.left(request.size() - 2);
    const quint16 expectedCrc = crc16(payloadWithoutCrc);
    const quint16 actualCrc = static_cast<quint8>(request.at(request.size() - 2))
                              | (static_cast<quint8>(request.at(request.size() - 1)) << 8);
    if (expectedCrc != actualCrc) {
        emit logMessage(QStringLiteral("CRC 校验失败：期望=%1，实际=%2")
                            .arg(expectedCrc, 4, 16, QLatin1Char('0'))
                            .arg(actualCrc, 4, 16, QLatin1Char('0')));
        return {};
    }

    auto readU16 = [](const QByteArray &data, int offset) -> quint16 {
        return (static_cast<quint8>(data.at(offset)) << 8) | static_cast<quint8>(data.at(offset + 1));
    };

    switch (functionCode) {
    case 0x03: {
        if (request.size() != 8) {
            emit logMessage(QStringLiteral("0x03 请求长度非法。"));
            return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataValue);
        }
        const quint16 start = readU16(request, 2);
        const quint16 quantity = readU16(request, 4);
        emit logMessage(QStringLiteral("解析 0x03: 起始地址=%1, 数量=%2").arg(start).arg(quantity));
        QVector<quint16> values;
        if (!m_store->readHoldingRegisters(start, quantity, values)) {
            return buildExceptionResponse(functionCode,
                                          quantity == 0 || quantity > 125
                                              ? ExceptionCode::IllegalDataValue
                                              : ExceptionCode::IllegalDataAddress);
        }
        QByteArray payload;
        payload.append(static_cast<char>(values.size() * 2));
        for (quint16 value : values) {
            payload.append(static_cast<char>((value >> 8) & 0xFF));
            payload.append(static_cast<char>(value & 0xFF));
        }
        return buildNormalResponse(functionCode, payload);
    }
    case 0x06: {
        if (request.size() != 8) {
            emit logMessage(QStringLiteral("0x06 请求长度非法。"));
            return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataValue);
        }
        const quint16 address = readU16(request, 2);
        const quint16 value = readU16(request, 4);
        emit logMessage(QStringLiteral("解析 0x06: 地址=%1, 值=%2").arg(address).arg(value));
        QString error;
        if (!m_store->writeSingleHoldingRegister(address, value, &error)) {
            emit logMessage(QStringLiteral("0x06 写入失败：%1").arg(error));
            if (error.contains(QStringLiteral("非法")) || error.contains(QStringLiteral("数量"))) {
                return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataValue);
            }
            if (error.contains(QStringLiteral("只读"))) {
                return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataAddress);
            }
            return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataAddress);
        }
        return request;
    }
    case 0x10: {
        if (request.size() < 9) {
            emit logMessage(QStringLiteral("0x10 请求长度非法。"));
            return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataValue);
        }
        const quint16 start = readU16(request, 2);
        const quint16 quantity = readU16(request, 4);
        const quint8 byteCount = static_cast<quint8>(request.at(6));
        emit logMessage(QStringLiteral("解析 0x10: 起始地址=%1, 数量=%2, 字节数=%3").arg(start).arg(quantity).arg(byteCount));
        if (quantity == 0 || quantity > 123 || byteCount != quantity * 2 || request.size() != 9 + byteCount) {
            return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataValue);
        }
        QVector<quint16> values;
        values.reserve(quantity);
        for (int i = 0; i < quantity; ++i) {
            values.append(readU16(request, 7 + i * 2));
        }
        QString error;
        if (!m_store->writeMultipleHoldingRegisters(start, values, &error)) {
            emit logMessage(QStringLiteral("0x10 写入失败：%1").arg(error));
            if (error.contains(QStringLiteral("数量"))) {
                return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataValue);
            }
            return buildExceptionResponse(functionCode, ExceptionCode::IllegalDataAddress);
        }
        QByteArray payload;
        payload.append(static_cast<char>((start >> 8) & 0xFF));
        payload.append(static_cast<char>(start & 0xFF));
        payload.append(static_cast<char>((quantity >> 8) & 0xFF));
        payload.append(static_cast<char>(quantity & 0xFF));
        return buildNormalResponse(functionCode, payload);
    }
    case 0x01:
    case 0x02:
        emit logMessage(QStringLiteral("功能码 0x%1 当前仅保留架构，返回非法功能码异常。")
                            .arg(functionCode, 2, 16, QLatin1Char('0')));
        return buildExceptionResponse(functionCode, ExceptionCode::IllegalFunction);
    default:
        emit logMessage(QStringLiteral("收到不支持的功能码 0x%1").arg(functionCode, 2, 16, QLatin1Char('0')));
        return buildExceptionResponse(functionCode, ExceptionCode::IllegalFunction);
    }
}

QByteArray ModbusRtuSlave::buildExceptionResponse(quint8 functionCode, ExceptionCode code) const
{
    QByteArray response;
    response.append(static_cast<char>(m_slaveId));
    response.append(static_cast<char>(functionCode | 0x80));
    response.append(static_cast<char>(code));
    const quint16 crc = crc16(response);
    response.append(static_cast<char>(crc & 0xFF));
    response.append(static_cast<char>((crc >> 8) & 0xFF));
    return response;
}

QByteArray ModbusRtuSlave::buildNormalResponse(quint8 functionCode, const QByteArray &payload) const
{
    QByteArray response;
    response.append(static_cast<char>(m_slaveId));
    response.append(static_cast<char>(functionCode));
    response.append(payload);
    const quint16 crc = crc16(response);
    response.append(static_cast<char>(crc & 0xFF));
    response.append(static_cast<char>((crc >> 8) & 0xFF));
    return response;
}
