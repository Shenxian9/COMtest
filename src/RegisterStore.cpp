#include "RegisterStore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>

#include <cstring>

namespace {
constexpr quint16 kMaxReadRegisters = 125;
constexpr quint16 kMaxWriteRegisters = 123;
}

RegisterStore::RegisterStore(QObject *parent)
    : QObject(parent)
{
    initializeDefinitions();
    initializeDefaults();
}

QVector<RegisterEntry> RegisterStore::entries() const
{
    return m_entries;
}

const RegisterEntry *RegisterStore::entryByAddress(uint16_t address) const
{
    auto it = m_entryByAddress.constFind(address);
    if (it == m_entryByAddress.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

bool RegisterStore::readHoldingRegisters(uint16_t start, uint16_t quantity, QVector<quint16> &values) const
{
    if (quantity == 0 || quantity > kMaxReadRegisters || !containsRange(start, quantity)) {
        return false;
    }

    values.clear();
    values.reserve(quantity);
    for (uint16_t i = 0; i < quantity; ++i) {
        values.append(reg(start + i));
    }
    return true;
}

bool RegisterStore::writeSingleHoldingRegister(uint16_t address, quint16 value, QString *errorMessage)
{
    return writeMultipleHoldingRegisters(address, QVector<quint16>{value}, errorMessage);
}

bool RegisterStore::writeMultipleHoldingRegisters(uint16_t start, const QVector<quint16> &values, QString *errorMessage)
{
    if (values.isEmpty() || values.size() > kMaxWriteRegisters) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("写入数量非法");
        }
        return false;
    }

    if (!containsRange(start, static_cast<uint16_t>(values.size()))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("地址范围非法");
        }
        return false;
    }

    if (!isRangeWritable(start, static_cast<uint16_t>(values.size()), errorMessage)) {
        return false;
    }

    QVector<uint16_t> changed;
    changed.reserve(values.size());
    for (int i = 0; i < values.size(); ++i) {
        const uint16_t address = start + static_cast<uint16_t>(i);
        setReg(address, values.at(i));
        changed.append(address);
    }

    emit registersChanged(changed);
    return true;
}

QString RegisterStore::displayValue(uint16_t startAddress) const
{
    const RegisterEntry *entry = entryByAddress(startAddress);
    if (!entry) {
        return QString();
    }

    switch (entry->format) {
    case RegisterFormat::U16:
        return QString::number(reg(startAddress));
    case RegisterFormat::I16:
        return QString::number(static_cast<qint16>(reg(startAddress)));
    case RegisterFormat::U32:
        return QString::number(getU32(startAddress));
    case RegisterFormat::Float:
        return QString::number(getFloat(startAddress), 'f', 3);
    case RegisterFormat::Ascii:
    case RegisterFormat::Raw: {
        QStringList list;
        for (uint16_t i = 0; i < entry->count; ++i) {
            list.append(QString::number(reg(startAddress + i)));
        }
        return list.join(QStringLiteral(", "));
    }
    }
    return QString();
}

bool RegisterStore::setDisplayValue(uint16_t startAddress, const QString &text, QString *errorMessage)
{
    const RegisterEntry *entry = entryByAddress(startAddress);
    if (!entry) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未找到寄存器定义");
        }
        return false;
    }

    bool ok = false;
    switch (entry->format) {
    case RegisterFormat::U16: {
        const auto value = text.toUShort(&ok);
        if (!ok) break;
        setReg(startAddress, value);
        emit registersChanged({startAddress});
        return true;
    }
    case RegisterFormat::I16: {
        const auto value = text.toShort(&ok);
        if (!ok) break;
        setReg(startAddress, static_cast<quint16>(value));
        emit registersChanged({startAddress});
        return true;
    }
    case RegisterFormat::U32: {
        const auto value = text.toUInt(&ok);
        if (!ok) break;
        setU32(startAddress, value);
        emit registersChanged({startAddress, static_cast<uint16_t>(startAddress + 1)});
        return true;
    }
    case RegisterFormat::Float: {
        const float value = text.toFloat(&ok);
        if (!ok) break;
        setFloat(startAddress, value);
        emit registersChanged({startAddress, static_cast<uint16_t>(startAddress + 1)});
        return true;
    }
    case RegisterFormat::Ascii:
    case RegisterFormat::Raw: {
        const QStringList parts = text.split(',', Qt::SkipEmptyParts);
        if (parts.size() != entry->count) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("请输入与寄存器数量一致的逗号分隔值");
            }
            return false;
        }
        QVector<uint16_t> changed;
        for (int i = 0; i < parts.size(); ++i) {
            const auto value = parts.at(i).trimmed().toUShort(&ok);
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("存在无法解析的整数值");
                }
                return false;
            }
            const auto address = static_cast<uint16_t>(startAddress + i);
            setReg(address, value);
            changed.append(address);
        }
        emit registersChanged(changed);
        return true;
    }
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("输入值格式错误");
    }
    return false;
}

RegisterStore::QuickDebugValues RegisterStore::quickDebugValues() const
{
    QuickDebugValues values;
    values.instantFlow = getFloat(kAddrInstantFlow);
    values.instantVelocity = getFloat(kAddrInstantVelocity);
    values.zeroCutoff = getFloat(kAddrZeroCutoff);
    values.pipeOuterDiameter = getFloat(kAddrPipeOuterDiameter);
    values.pipeWallThickness = getFloat(kAddrPipeWallThickness);
    values.calibrationFactor = getFloat(kAddrCalibrationFactor);
    values.analogUpper = getFloat(kAddrAnalogUpper);
    values.analogLower = getFloat(kAddrAnalogLower);
    values.alarmUpper = getFloat(kAddrAlarmUpper);
    values.alarmLower = getFloat(kAddrAlarmLower);
    values.fixedErrorCompensation = getFloat(kAddrFixedErrorComp);
    values.pulseEquivalent = reg(kAddrPulseEquivalent);
    values.serialYear = reg(kAddrSerialYear);
    values.serialWeek = reg(kAddrSerialWeek);
    values.serialProductNumber = reg(kAddrSerialProductNumber);
    values.serialSequence = reg(kAddrSerialSequence);
    values.outputMode = reg(kAddrOutputMode);
    values.pipeMaterial = reg(kAddrPipeMaterial);
    values.language = reg(kAddrLanguage);
    values.sensitivity = reg(kAddrSensitivity);
    values.screenOrientation = reg(kAddrScreenOrientation);
    values.baudRate = reg(kAddrBaudRate);
    values.dataBits = reg(kAddrDataBits);
    values.parity = reg(kAddrParity);
    values.stopBits = reg(kAddrStopBits);
    values.temperatureMeasurementEnabled = reg(kAddrTemperatureMeasurementEnabled);
    return values;
}

void RegisterStore::applyQuickDebugValues(const QuickDebugValues &values)
{
    setFloat(kAddrInstantFlow, values.instantFlow);
    setFloat(kAddrInstantVelocity, values.instantVelocity);
    setFloat(kAddrZeroCutoff, values.zeroCutoff);
    setFloat(kAddrPipeOuterDiameter, values.pipeOuterDiameter);
    setFloat(kAddrPipeWallThickness, values.pipeWallThickness);
    setFloat(kAddrCalibrationFactor, values.calibrationFactor);
    setFloat(kAddrAnalogUpper, values.analogUpper);
    setFloat(kAddrAnalogLower, values.analogLower);
    setFloat(kAddrAlarmUpper, values.alarmUpper);
    setFloat(kAddrAlarmLower, values.alarmLower);
    setFloat(kAddrFixedErrorComp, values.fixedErrorCompensation);
    setReg(kAddrPulseEquivalent, values.pulseEquivalent);
    setReg(kAddrSerialYear, values.serialYear);
    setReg(kAddrSerialWeek, values.serialWeek);
    setReg(kAddrSerialProductNumber, values.serialProductNumber);
    setReg(kAddrSerialSequence, values.serialSequence);
    setReg(kAddrOutputMode, values.outputMode);
    setReg(kAddrPipeMaterial, values.pipeMaterial);
    setReg(kAddrLanguage, values.language);
    setReg(kAddrSensitivity, values.sensitivity);
    setReg(kAddrScreenOrientation, values.screenOrientation);
    setReg(kAddrBaudRate, values.baudRate);
    setReg(kAddrDataBits, values.dataBits);
    setReg(kAddrParity, values.parity);
    setReg(kAddrStopBits, values.stopBits);
    setReg(kAddrTemperatureMeasurementEnabled, values.temperatureMeasurementEnabled);

    QVector<uint16_t> changed;
    for (const auto &entry : m_entries) {
        for (uint16_t i = 0; i < entry.count; ++i) {
            changed.append(entry.address + i);
        }
    }
    emit registersChanged(changed);
}

void RegisterStore::loadDefaultSimulationData()
{
    initializeDefaults();
    QVector<uint16_t> changed;
    for (const auto &entry : m_entries) {
        for (uint16_t i = 0; i < entry.count; ++i) {
            changed.append(entry.address + i);
        }
    }
    emit registersChanged(changed);
    emit logMessage(QStringLiteral("已加载默认模拟数据。"));
}

void RegisterStore::randomizeInstantValues()
{
    const float flow = static_cast<float>(QRandomGenerator::global()->generateDouble() * 200.0);
    const float velocity = static_cast<float>(QRandomGenerator::global()->generateDouble() * 10.0);
    setFloat(kAddrInstantFlow, flow);
    setFloat(kAddrInstantVelocity, velocity);
    emit registersChanged({kAddrInstantFlow, static_cast<uint16_t>(kAddrInstantFlow + 1),
                           kAddrInstantVelocity, static_cast<uint16_t>(kAddrInstantVelocity + 1)});
    emit logMessage(QStringLiteral("已随机更新瞬时流量与瞬时流速。"));
}

void RegisterStore::restoreFactoryDefaults()
{
    initializeDefaults();
    emit logMessage(QStringLiteral("已恢复出厂默认值。"));
    QVector<uint16_t> changed;
    for (const auto &entry : m_entries) {
        for (uint16_t i = 0; i < entry.count; ++i) {
            changed.append(entry.address + i);
        }
    }
    emit registersChanged(changed);
}

bool RegisterStore::saveSnapshot(const QString &filePath, QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    return true;
}

bool RegisterStore::loadSnapshot(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("JSON 格式无效");
        }
        return false;
    }

    if (!fromJson(doc.object(), errorMessage)) {
        return false;
    }

    QVector<uint16_t> changed;
    for (const auto &entry : m_entries) {
        for (uint16_t i = 0; i < entry.count; ++i) {
            changed.append(entry.address + i);
        }
    }
    emit registersChanged(changed);
    return true;
}

void RegisterStore::initializeDefinitions()
{
    m_entries.clear();
    m_entryByAddress.clear();

    defineRegister({kAddrInstantFlow, QStringLiteral("瞬时流量"), RegisterKind::Holding, 2, RegisterFormat::Float, false, true,
                    QStringLiteral("浮点数（低字在前、高字在后），瞬时流量，用于调试。"), false});
    defineRegister({kAddrInstantVelocity, QStringLiteral("瞬时流速"), RegisterKind::Holding, 2, RegisterFormat::Float, false, true,
                    QStringLiteral("浮点数（低字在前、高字在后），瞬时流速，用于调试。"), false});
    defineRegister({kAddrZeroCutoff, QStringLiteral("零切下限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("零切值，程序单位（立方米每小时）。"), false});
    defineRegister({kAddrPipeOuterDiameter, QStringLiteral("管道外径"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("单位 mm。"), false});
    defineRegister({kAddrPipeWallThickness, QStringLiteral("管道壁厚"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("单位 mm。"), false});
    defineRegister({kAddrCalibrationFactor, QStringLiteral("校准系数"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("校准系数，默认 1.00。"), false});
    defineRegister({kAddrAnalogUpper, QStringLiteral("模拟上限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("程序单位（立方米每小时）。"), false});
    defineRegister({kAddrAnalogLower, QStringLiteral("模拟下限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("程序单位（立方米每小时）。"), false});
    defineRegister({kAddrAlarmUpper, QStringLiteral("报警上限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("程序单位（立方米每小时）。"), false});
    defineRegister({kAddrAlarmLower, QStringLiteral("报警下限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("程序单位（立方米每小时）。"), false});
    defineRegister({kAddrFixedErrorComp, QStringLiteral("固定误差补偿"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("算法补偿值。"), false});
    defineRegister({kAddrPulseEquivalent, QStringLiteral("脉冲当量"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=0.1mL，1=1mL，2=10mL，3=100mL，4=1L，5=10L，6=100L，7=1000L。"), false});
    defineRegister({kAddrSerialYear, QStringLiteral("序列号-生产年份"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("生产年份。"), false});
    defineRegister({kAddrSerialWeek, QStringLiteral("序列号-生产周"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("生产周。"), false});
    defineRegister({kAddrSerialProductNumber, QStringLiteral("序列号-产品编号"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("产品编号。"), false});
    defineRegister({kAddrSerialSequence, QStringLiteral("序列号-生产序列"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("生产序列。"), false});
    defineRegister({kAddrOutputMode, QStringLiteral("输出模式"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=4-20mA，1=PNP脉冲，2=NPN脉冲，3=报警输出。"), false});
    defineRegister({kAddrPipeMaterial, QStringLiteral("管道材质"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=PVC，1=金属，2=合金。"), false});
    defineRegister({kAddrLanguage, QStringLiteral("语言"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("1=简体中文，0=English。"), false});
    defineRegister({kAddrSensitivity, QStringLiteral("灵敏度"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=低，1=中，2=高。"), false});
    defineRegister({kAddrScreenOrientation, QStringLiteral("屏幕方向"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=正向，1=旋转90°，2=旋转180°，4=旋转270°。"), false});
    defineRegister({kAddrBaudRate, QStringLiteral("波特率"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=2400，1=4800，2=9600，4=19200，5=38400，6=115200；重启生效。"), false});
    defineRegister({kAddrDataBits, QStringLiteral("数据位"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=8位，1=7位；重启生效。"), false});
    defineRegister({kAddrParity, QStringLiteral("校验位"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=无校验，1=奇校验，2=偶校验；重启生效。"), false});
    defineRegister({kAddrStopBits, QStringLiteral("停止位"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=1位，1=2位；重启生效。"), false});
    defineRegister({kAddrTemperatureMeasurementEnabled, QStringLiteral("测温使能"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=不测温，1=测温；重启生效。"), false});
}

void RegisterStore::initializeDefaults()
{
    m_registers.clear();
    for (const auto &entry : m_entries) {
        for (uint16_t i = 0; i < entry.count; ++i) {
            m_registers.insert(entry.address + i, 0);
        }
    }

    setFloat(kAddrInstantFlow, 12.34f);
    setFloat(kAddrInstantVelocity, 1.23f);
    setFloat(kAddrZeroCutoff, 0.10f);
    setFloat(kAddrPipeOuterDiameter, 108.0f);
    setFloat(kAddrPipeWallThickness, 4.5f);
    setFloat(kAddrCalibrationFactor, 1.00f);
    setFloat(kAddrAnalogUpper, 100.0f);
    setFloat(kAddrAnalogLower, 0.0f);
    setFloat(kAddrAlarmUpper, 120.0f);
    setFloat(kAddrAlarmLower, -10.0f);
    setFloat(kAddrFixedErrorComp, 0.0f);
    setReg(kAddrPulseEquivalent, 4);
    setReg(kAddrSerialYear, 2026);
    setReg(kAddrSerialWeek, 12);
    setReg(kAddrSerialProductNumber, 1001);
    setReg(kAddrSerialSequence, 1);
    setReg(kAddrOutputMode, 0);
    setReg(kAddrPipeMaterial, 1);
    setReg(kAddrLanguage, 1);
    setReg(kAddrSensitivity, 1);
    setReg(kAddrScreenOrientation, 0);
    setReg(kAddrBaudRate, 2);
    setReg(kAddrDataBits, 0);
    setReg(kAddrParity, 0);
    setReg(kAddrStopBits, 0);
    setReg(kAddrTemperatureMeasurementEnabled, 0);
}

void RegisterStore::defineRegister(const RegisterEntry &entry)
{
    m_entries.append(entry);
    m_entryByAddress.insert(entry.address, entry);
}

bool RegisterStore::containsRange(uint16_t start, uint16_t count) const
{
    for (uint16_t i = 0; i < count; ++i) {
        if (!m_registers.contains(start + i)) {
            return false;
        }
    }
    return true;
}

bool RegisterStore::isRangeWritable(uint16_t start, uint16_t count, QString *errorMessage) const
{
    for (uint16_t i = 0; i < count; ++i) {
        const auto *entry = entryContainingAddress(start + i);
        if (!entry) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("未找到寄存器定义");
            }
            return false;
        }
        if (!entry->writableByModbus) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("目标寄存器为只读");
            }
            return false;
        }
    }
    return true;
}

const RegisterEntry *RegisterStore::entryContainingAddress(uint16_t address) const
{
    for (const auto &entry : m_entries) {
        if (address >= entry.address && address < entry.address + entry.count) {
            return &entry;
        }
    }
    return nullptr;
}

quint16 RegisterStore::reg(uint16_t address) const
{
    return m_registers.value(address, 0);
}

void RegisterStore::setReg(uint16_t address, quint16 value)
{
    m_registers[address] = value;
}

float RegisterStore::getFloat(uint16_t address) const
{
    const quint32 raw = getU32(address);
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(float));
    return value;
}

void RegisterStore::setFloat(uint16_t address, float value)
{
    quint32 raw = 0;
    std::memcpy(&raw, &value, sizeof(float));
    setU32(address, raw);
}

quint32 RegisterStore::getU32(uint16_t address) const
{
    return (static_cast<quint32>(reg(address + 1)) << 16) | reg(address);
}

void RegisterStore::setU32(uint16_t address, quint32 value)
{
    setReg(address, static_cast<quint16>(value & 0xFFFF));
    setReg(address + 1, static_cast<quint16>((value >> 16) & 0xFFFF));
}

QJsonObject RegisterStore::toJson() const
{
    QJsonObject root;
    QJsonArray registerArray;
    for (auto it = m_registers.constBegin(); it != m_registers.constEnd(); ++it) {
        QJsonObject item;
        item[QStringLiteral("address")] = static_cast<int>(it.key());
        item[QStringLiteral("value")] = static_cast<int>(it.value());
        registerArray.append(item);
    }
    root[QStringLiteral("registers")] = registerArray;
    return root;
}

bool RegisterStore::fromJson(const QJsonObject &json, QString *errorMessage)
{
    if (!json.contains(QStringLiteral("registers")) || !json.value(QStringLiteral("registers")).isArray()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("缺少 registers 数组");
        }
        return false;
    }

    initializeDefaults();
    const auto array = json.value(QStringLiteral("registers")).toArray();
    for (const auto &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const auto obj = value.toObject();
        const uint16_t address = static_cast<uint16_t>(obj.value(QStringLiteral("address")).toInt());
        const quint16 regValue = static_cast<quint16>(obj.value(QStringLiteral("value")).toInt());
        if (m_registers.contains(address)) {
            m_registers[address] = regValue;
        }
    }
    return true;
}
