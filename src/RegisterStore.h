#pragma once

#include "RegisterEntry.h"

#include <QObject>
#include <QJsonObject>
#include <QMap>
#include <QVector>

class RegisterStore : public QObject
{
    Q_OBJECT

public:
    struct QuickDebugValues {
        float instantFlow = 12.34f;
        float instantVelocity = 1.23f;
        float zeroCutoff = 0.10f;
        float pipeOuterDiameter = 108.0f;
        float pipeWallThickness = 4.5f;
        float calibrationFactor = 1.00f;
        float analogUpper = 100.0f;
        float analogLower = 0.0f;
        float alarmUpper = 120.0f;
        float alarmLower = -10.0f;
        float fixedErrorCompensation = 0.0f;
        quint16 pulseEquivalent = 4;
        quint16 serialYear = 2026;
        quint16 serialWeek = 12;
        quint16 serialProductNumber = 1001;
        quint16 serialSequence = 1;
        quint16 outputMode = 0;
        quint16 pipeMaterial = 1;
        quint16 language = 1;
        quint16 sensitivity = 1;
        quint16 screenOrientation = 0;
        quint16 baudRate = 2;
        quint16 dataBits = 0;
        quint16 parity = 0;
        quint16 stopBits = 0;
        quint16 temperatureMeasurementEnabled = 0;
    };

    explicit RegisterStore(QObject *parent = nullptr);

    static constexpr uint16_t kAddrInstantFlow = 8192;
    static constexpr uint16_t kAddrInstantVelocity = 8194;
    static constexpr uint16_t kAddrZeroCutoff = 8196;
    static constexpr uint16_t kAddrPipeOuterDiameter = 8198;
    static constexpr uint16_t kAddrPipeWallThickness = 8200;
    static constexpr uint16_t kAddrCalibrationFactor = 8202;
    static constexpr uint16_t kAddrAnalogUpper = 8204;
    static constexpr uint16_t kAddrAnalogLower = 8206;
    static constexpr uint16_t kAddrAlarmUpper = 8208;
    static constexpr uint16_t kAddrAlarmLower = 8210;
    static constexpr uint16_t kAddrFixedErrorComp = 8212;
    static constexpr uint16_t kAddrPulseEquivalent = 8214;
    static constexpr uint16_t kAddrSerialYear = 8215;
    static constexpr uint16_t kAddrSerialWeek = 8216;
    static constexpr uint16_t kAddrSerialProductNumber = 8217;
    static constexpr uint16_t kAddrSerialSequence = 8218;
    static constexpr uint16_t kAddrOutputMode = 8219;
    static constexpr uint16_t kAddrPipeMaterial = 8220;
    static constexpr uint16_t kAddrLanguage = 8221;
    static constexpr uint16_t kAddrSensitivity = 8222;
    static constexpr uint16_t kAddrScreenOrientation = 8223;
    static constexpr uint16_t kAddrBaudRate = 8224;
    static constexpr uint16_t kAddrDataBits = 8225;
    static constexpr uint16_t kAddrParity = 8226;
    static constexpr uint16_t kAddrStopBits = 8227;
    static constexpr uint16_t kAddrTemperatureMeasurementEnabled = 8228;

    QVector<RegisterEntry> entries() const;
    const RegisterEntry *entryByAddress(uint16_t address) const;

    bool readHoldingRegisters(uint16_t start, uint16_t quantity, QVector<quint16> &values) const;
    bool writeSingleHoldingRegister(uint16_t address, quint16 value, QString *errorMessage = nullptr);
    bool writeMultipleHoldingRegisters(uint16_t start, const QVector<quint16> &values, QString *errorMessage = nullptr);

    QString displayValue(uint16_t startAddress) const;
    bool setDisplayValue(uint16_t startAddress, const QString &text, QString *errorMessage = nullptr);

    QuickDebugValues quickDebugValues() const;
    void applyQuickDebugValues(const QuickDebugValues &values);
    void loadDefaultSimulationData();
    void randomizeInstantValues();
    void restoreFactoryDefaults();

    bool saveSnapshot(const QString &filePath, QString *errorMessage = nullptr) const;
    bool loadSnapshot(const QString &filePath, QString *errorMessage = nullptr);

signals:
    void registersChanged(const QVector<uint16_t> &addresses);
    void logMessage(const QString &message);

private:
    void initializeDefinitions();
    void initializeDefaults();
    void defineRegister(const RegisterEntry &entry);

    bool containsRange(uint16_t start, uint16_t count) const;
    bool isRangeWritable(uint16_t start, uint16_t count, QString *errorMessage = nullptr) const;
    const RegisterEntry *entryContainingAddress(uint16_t address) const;

    quint16 reg(uint16_t address) const;
    void setReg(uint16_t address, quint16 value);

    float getFloat(uint16_t address) const;
    void setFloat(uint16_t address, float value);
    quint32 getU32(uint16_t address) const;
    void setU32(uint16_t address, quint32 value);

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json, QString *errorMessage = nullptr);

    QVector<RegisterEntry> m_entries;
    QMap<uint16_t, RegisterEntry> m_entryByAddress;
    QMap<uint16_t, quint16> m_registers;
};
