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
        quint16 language = 0;
        quint16 sensitivity = 1;
        float pipeOuterDiameter = 108.0f;
        float pipeWallThickness = 4.5f;
        quint16 pipeMaterial = 1;
        quint16 outputMode = 0;
        quint16 pulseEquivalent = 4;
        float analogUpper = 100.0f;
        float analogLower = 0.0f;
        float alarmUpper = 120.0f;
        float alarmLower = -10.0f;
        float fixedErrorCompensation = 0.0f;
    };

    struct AddressRange {
        uint16_t start = 0;
        uint16_t count = 0;
    };

    explicit RegisterStore(QObject *parent = nullptr);

    static constexpr uint16_t kAddrInstantFlow = 8192;
    static constexpr uint16_t kAddrInstantVelocity = 8194;
    static constexpr uint16_t kAddrZeroCutoff = 8196;
    static constexpr uint16_t kAddrSensitivity = 8198;
    static constexpr uint16_t kAddrPipeOuterDiameter = 8200;
    static constexpr uint16_t kAddrPipeWallThickness = 8202;
    static constexpr uint16_t kAddrPipeMaterial = 8203;

    static constexpr uint16_t kAddrTempBase = 8300;
    static constexpr uint16_t kAddrCalibrationFactor = 8300;
    static constexpr uint16_t kAddrPulseEquivalent = 8302;
    static constexpr uint16_t kAddrSerialYear = 8303;
    static constexpr uint16_t kAddrOutputMode = 8307;
    static constexpr uint16_t kAddrAnalogUpper = 8308;
    static constexpr uint16_t kAddrAnalogLower = 8310;
    static constexpr uint16_t kAddrAlarmUpper = 8312;
    static constexpr uint16_t kAddrAlarmLower = 8314;
    static constexpr uint16_t kAddrFixedErrorComp = 8316;
    static constexpr uint16_t kAddrPowerOnTotal = 8318;
    static constexpr uint16_t kAddrClearTotal = 8320;
    static constexpr uint16_t kAddrTablePoints = 8321;
    static constexpr uint16_t kAddrTableFactors = 8337;

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
    QVector<uint16_t> collectChangedAddresses(uint16_t start, uint16_t count) const;

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
