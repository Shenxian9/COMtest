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

    if (start <= kAddrClearTotal && (start + values.size()) > kAddrClearTotal && reg(kAddrClearTotal) == 1) {
        setFloat(kAddrPowerOnTotal, 0.0f);
        setReg(kAddrClearTotal, 0);
        changed.append(kAddrPowerOnTotal);
        changed.append(kAddrPowerOnTotal + 1);
        changed.append(kAddrClearTotal);
        emit logMessage(QStringLiteral("检测到“累积量清零”命令，已将上电累计清零。"));
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
    values.language = reg(8197);
    values.sensitivity = reg(kAddrSensitivity);
    values.pipeOuterDiameter = getFloat(kAddrPipeOuterDiameter);
    values.pipeWallThickness = getFloat(kAddrPipeWallThickness);
    values.pipeMaterial = reg(kAddrPipeMaterial);
    values.outputMode = reg(kAddrOutputMode);
    values.pulseEquivalent = reg(kAddrPulseEquivalent);
    values.analogUpper = getFloat(kAddrAnalogUpper);
    values.analogLower = getFloat(kAddrAnalogLower);
    values.alarmUpper = getFloat(kAddrAlarmUpper);
    values.alarmLower = getFloat(kAddrAlarmLower);
    values.fixedErrorCompensation = getFloat(kAddrFixedErrorComp);
    return values;
}

void RegisterStore::applyQuickDebugValues(const QuickDebugValues &values)
{
    setFloat(kAddrInstantFlow, values.instantFlow);
    setFloat(kAddrInstantVelocity, values.instantVelocity);
    setFloat(kAddrZeroCutoff, values.zeroCutoff);
    setReg(8197, values.language);
    setReg(kAddrSensitivity, values.sensitivity);
    setFloat(kAddrPipeOuterDiameter, values.pipeOuterDiameter);
    setFloat(kAddrPipeWallThickness, values.pipeWallThickness);
    setReg(kAddrPipeMaterial, values.pipeMaterial);
    setReg(kAddrOutputMode, values.outputMode);
    setReg(kAddrPulseEquivalent, values.pulseEquivalent);
    setFloat(kAddrAnalogUpper, values.analogUpper);
    setFloat(kAddrAnalogLower, values.analogLower);
    setFloat(kAddrAlarmUpper, values.alarmUpper);
    setFloat(kAddrAlarmLower, values.alarmLower);
    setFloat(kAddrFixedErrorComp, values.fixedErrorCompensation);

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

    defineRegister({8192, QStringLiteral("瞬时流量"), RegisterKind::Holding, 2, RegisterFormat::Float, false, true,
                    QStringLiteral("实时模拟值，float，大端。"), false});
    defineRegister({8194, QStringLiteral("瞬时流速"), RegisterKind::Holding, 2, RegisterFormat::Float, false, true,
                    QStringLiteral("实时模拟值，float，大端。"), false});
    defineRegister({8196, QStringLiteral("零切下限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("调试参数，按 float 处理。"), false});
    // 需求文字中 8196 后接 8197/8198/8200/8202/8203，存在地址连续性歧义；本模拟器严格按用户给定地址实现。
    defineRegister({8197, QStringLiteral("语言"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=简体中文，1=English。"), false});
    defineRegister({8198, QStringLiteral("灵敏度"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=低，1=中，2=高。"), false});
    defineRegister({8200, QStringLiteral("管道外径"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("单位 mm。"), false});
    defineRegister({8202, QStringLiteral("管道壁厚"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("单位 mm。"), false});
    defineRegister({8203, QStringLiteral("管道材质"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("0=PVC，1=金属，2=合金。"), false});

    defineRegister({kAddrCalibrationFactor, QStringLiteral("校准系数"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("临时模拟地址：文档未明确起始地址。"), true});
    defineRegister({kAddrPulseEquivalent, QStringLiteral("脉冲当量"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("临时模拟地址：0=0.1mL, 1=1mL, 2=10mL, 3=100mL, 4=1L, 5=10L, 6=100L, 7=1000L。"), true});
    // 文档一处写“共6字节”，另一处写 4 个 16 位字段。此处按 4x16bit=8 字节实现，更便于 Modbus 映射。
    defineRegister({kAddrSerialYear, QStringLiteral("序列号字段"), RegisterKind::Holding, 4, RegisterFormat::Raw, true, true,
                    QStringLiteral("临时模拟地址：依次为生产年份、生产周、产品型号、生产序列。文档对此长度描述存在歧义。"), true});
    defineRegister({kAddrOutputMode, QStringLiteral("输出模式"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("临时模拟地址：0=4-20mA，1=PNP脉冲，2=NPN脉冲，3=报警输出。"), true});
    defineRegister({kAddrAnalogUpper, QStringLiteral("模拟上限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("临时模拟地址。"), true});
    defineRegister({kAddrAnalogLower, QStringLiteral("模拟下限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("临时模拟地址。"), true});
    defineRegister({kAddrAlarmUpper, QStringLiteral("报警上限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("临时模拟地址。"), true});
    defineRegister({kAddrAlarmLower, QStringLiteral("报警下限"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("临时模拟地址。"), true});
    defineRegister({kAddrFixedErrorComp, QStringLiteral("固定误差补偿"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("临时模拟地址。"), true});
    defineRegister({kAddrPowerOnTotal, QStringLiteral("上电累计"), RegisterKind::Holding, 2, RegisterFormat::Float, true, true,
                    QStringLiteral("临时模拟地址。"), true});
    defineRegister({kAddrClearTotal, QStringLiteral("累积量清零"), RegisterKind::Holding, 1, RegisterFormat::U16, true, true,
                    QStringLiteral("临时模拟地址，写 1 时自动将上电累计清零。"), true});
    defineRegister({kAddrTablePoints, QStringLiteral("表格(8个点)"), RegisterKind::Holding, 16, RegisterFormat::Raw, true, true,
                    QStringLiteral("临时模拟地址，每点占 2 个寄存器。"), true});
    defineRegister({kAddrTableFactors, QStringLiteral("表格(8个修正系数)"), RegisterKind::Holding, 16, RegisterFormat::Raw, true, true,
                    QStringLiteral("临时模拟地址，每个系数占 2 个寄存器。"), true});
}

void RegisterStore::initializeDefaults()
{
    m_registers.clear();
    for (const auto &entry : m_entries) {
        for (uint16_t i = 0; i < entry.count; ++i) {
            m_registers.insert(entry.address + i, 0);
        }
    }

    setFloat(8192, 12.34f);
    setFloat(8194, 1.23f);
    setFloat(8196, 0.10f);
    setReg(8197, 0);
    setReg(8198, 1);
    setFloat(8200, 108.0f);
    setFloat(8202, 4.5f);
    setReg(8203, 1);

    setFloat(kAddrCalibrationFactor, 1.0f);
    setReg(kAddrPulseEquivalent, 4);
    setReg(kAddrSerialYear, 2026);
    setReg(kAddrSerialYear + 1, 12);
    setReg(kAddrSerialYear + 2, 1001);
    setReg(kAddrSerialYear + 3, 1);
    setReg(kAddrOutputMode, 0);
    setFloat(kAddrAnalogUpper, 100.0f);
    setFloat(kAddrAnalogLower, 0.0f);
    setFloat(kAddrAlarmUpper, 120.0f);
    setFloat(kAddrAlarmLower, -10.0f);
    setFloat(kAddrFixedErrorComp, 0.0f);
    setFloat(kAddrPowerOnTotal, 1234.5f);
    setReg(kAddrClearTotal, 0);

    for (int i = 0; i < 8; ++i) {
        setFloat(kAddrTablePoints + i * 2, static_cast<float>(i) * 10.0f);
        setFloat(kAddrTableFactors + i * 2, 1.0f + static_cast<float>(i) * 0.01f);
    }
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
    for (const auto &entry : m_entries) {
        if (start >= entry.address && start < entry.address + entry.count) {
            if (start + count > entry.address + entry.count) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("写入不能跨越寄存器定义边界");
                }
                return false;
            }
            if (!entry.writableByModbus) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("目标寄存器为只读");
                }
                return false;
            }
            return true;
        }
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("未找到可写寄存器定义");
    }
    return false;
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
    return (static_cast<quint32>(reg(address)) << 16) | reg(address + 1);
}

void RegisterStore::setU32(uint16_t address, quint32 value)
{
    setReg(address, static_cast<quint16>((value >> 16) & 0xFFFF));
    setReg(address + 1, static_cast<quint16>(value & 0xFFFF));
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
