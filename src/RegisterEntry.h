#pragma once

#include <QString>

#include <cstdint>

// 寄存器类型定义，程序内部按标准 Modbus 术语区分。
enum class RegisterKind {
    Holding,
    Input,
    Coil,
    DiscreteInput
};

// 寄存器数据格式描述，底层统一使用 16-bit 寄存器存储。
enum class RegisterFormat {
    U16,
    I16,
    U32,
    Float,
    Ascii,
    Raw
};

// 寄存器定义条目，用于描述界面与协议层共同使用的元数据。
struct RegisterEntry {
    uint16_t address = 0;
    QString name;
    RegisterKind kind = RegisterKind::Holding;
    uint16_t count = 1;
    RegisterFormat format = RegisterFormat::U16;
    bool writableByModbus = false;
    bool editableInGui = false;
    QString description;
    bool temporaryAddress = false;
};

inline QString registerKindToString(RegisterKind kind)
{
    switch (kind) {
    case RegisterKind::Holding:
        return QStringLiteral("holding");
    case RegisterKind::Input:
        return QStringLiteral("input");
    case RegisterKind::Coil:
        return QStringLiteral("coil");
    case RegisterKind::DiscreteInput:
        return QStringLiteral("discrete");
    }
    return QStringLiteral("unknown");
}

inline QString registerFormatToString(RegisterFormat format)
{
    switch (format) {
    case RegisterFormat::U16:
        return QStringLiteral("u16");
    case RegisterFormat::I16:
        return QStringLiteral("i16");
    case RegisterFormat::U32:
        return QStringLiteral("u32");
    case RegisterFormat::Float:
        return QStringLiteral("float");
    case RegisterFormat::Ascii:
        return QStringLiteral("ascii");
    case RegisterFormat::Raw:
        return QStringLiteral("raw");
    }
    return QStringLiteral("unknown");
}
