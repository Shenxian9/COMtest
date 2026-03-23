#pragma once

#include <QMainWindow>

#include "RegisterStore.h"

class ModbusRtuSlave;
class QComboBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshPorts();
    void toggleSerialPort();
    void refreshRegisterTable();
    void applyQuickDebugToRegisters();
    void loadQuickDebugFromRegisters();
    void onRegisterCellChanged(int row, int column);
    void saveSnapshot();
    void loadSnapshot();
    void appendLog(const QString &message);

private:
    void setupUi();
    QWidget *createSerialConfigGroup();
    QWidget *createDataGroup();
    QWidget *createQuickDebugGroup();
    QWidget *createLogGroup();

    void refreshQuickDebugControls(const RegisterStore::QuickDebugValues &values);
    void updateStatusLabel(const QString &message, bool opened);

    RegisterStore m_store;
    ModbusRtuSlave *m_slave = nullptr;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QSpinBox *m_slaveIdSpin = nullptr;
    QPushButton *m_togglePortButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QTableWidget *m_registerTable = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;

    QDoubleSpinBox *m_instantFlowSpin = nullptr;
    QDoubleSpinBox *m_instantVelocitySpin = nullptr;
    QDoubleSpinBox *m_zeroCutoffSpin = nullptr;
    QDoubleSpinBox *m_pipeOuterDiameterSpin = nullptr;
    QDoubleSpinBox *m_pipeWallThicknessSpin = nullptr;
    QDoubleSpinBox *m_calibrationFactorSpin = nullptr;
    QDoubleSpinBox *m_analogUpperSpin = nullptr;
    QDoubleSpinBox *m_analogLowerSpin = nullptr;
    QDoubleSpinBox *m_alarmUpperSpin = nullptr;
    QDoubleSpinBox *m_alarmLowerSpin = nullptr;
    QDoubleSpinBox *m_fixedErrorCompSpin = nullptr;

    QComboBox *m_pulseEquivalentCombo = nullptr;
    QSpinBox *m_serialYearSpin = nullptr;
    QSpinBox *m_serialWeekSpin = nullptr;
    QSpinBox *m_serialProductNumberSpin = nullptr;
    QSpinBox *m_serialSequenceSpin = nullptr;
    QComboBox *m_outputModeCombo = nullptr;
    QComboBox *m_pipeMaterialCombo = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QComboBox *m_sensitivityCombo = nullptr;
    QComboBox *m_screenOrientationCombo = nullptr;

    bool m_updatingTable = false;
};
