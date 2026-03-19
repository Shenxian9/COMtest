#include "MainWindow.h"

#include "ModbusRtuSlave.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QVBoxLayout>
#include <QDoubleSpinBox>

namespace {
QComboBox *createEnumCombo(const QStringList &items)
{
    auto *combo = new QComboBox;
    combo->addItems(items);
    return combo;
}

QDoubleSpinBox *createFloatSpin(double min, double max, double step = 0.1)
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(3);
    spin->setSingleStep(step);
    return spin;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_slave(new ModbusRtuSlave(&m_store, this))
{
    setupUi();
    refreshPorts();
    refreshRegisterTable();
    loadQuickDebugFromRegisters();

    connect(&m_store, &RegisterStore::registersChanged, this, [this](const QVector<uint16_t> &) {
        refreshRegisterTable();
        loadQuickDebugFromRegisters();
    });
    connect(&m_store, &RegisterStore::logMessage, this, &MainWindow::appendLog);
    connect(m_slave, &ModbusRtuSlave::logMessage, this, &MainWindow::appendLog);
    connect(m_slave, &ModbusRtuSlave::statusChanged, this, &MainWindow::updateStatusLabel);
}

void MainWindow::refreshPorts()
{
    const QString current = m_portCombo->currentText();
    m_portCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &port : ports) {
        m_portCombo->addItem(port.portName());
    }
    const int idx = m_portCombo->findText(current);
    if (idx >= 0) {
        m_portCombo->setCurrentIndex(idx);
    }
}

void MainWindow::toggleSerialPort()
{
    if (m_slave->isOpen()) {
        m_slave->closePort();
        return;
    }

    QString error;
    if (!m_slave->openPort(m_portCombo->currentText(),
                           m_baudCombo->currentText().toInt(),
                           static_cast<QSerialPort::DataBits>(m_dataBitsCombo->currentData().toInt()),
                           static_cast<QSerialPort::Parity>(m_parityCombo->currentData().toInt()),
                           static_cast<QSerialPort::StopBits>(m_stopBitsCombo->currentData().toInt()),
                           static_cast<quint8>(m_slaveIdSpin->value()),
                           &error)) {
        QMessageBox::critical(this, QStringLiteral("打开串口失败"), error);
    }
}

void MainWindow::refreshRegisterTable()
{
    m_updatingTable = true;
    const auto entries = m_store.entries();
    m_registerTable->setRowCount(entries.size());

    for (int row = 0; row < entries.size(); ++row) {
        const auto &entry = entries.at(row);
        auto *addressItem = new QTableWidgetItem(QString::number(entry.address));
        auto *nameItem = new QTableWidgetItem(entry.name + (entry.temporaryAddress ? QStringLiteral(" [临时]") : QString()));
        auto *kindItem = new QTableWidgetItem(registerKindToString(entry.kind));
        auto *countItem = new QTableWidgetItem(QString::number(entry.count));
        auto *formatItem = new QTableWidgetItem(registerFormatToString(entry.format));
        auto *valueItem = new QTableWidgetItem(m_store.displayValue(entry.address));
        auto *writableItem = new QTableWidgetItem(entry.writableByModbus ? QStringLiteral("是") : QStringLiteral("否"));
        auto *descItem = new QTableWidgetItem(entry.description);

        addressItem->setFlags(addressItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
        countItem->setFlags(countItem->flags() & ~Qt::ItemIsEditable);
        formatItem->setFlags(formatItem->flags() & ~Qt::ItemIsEditable);
        writableItem->setFlags(writableItem->flags() & ~Qt::ItemIsEditable);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        if (!entry.editableInGui) {
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        }

        m_registerTable->setItem(row, 0, addressItem);
        m_registerTable->setItem(row, 1, nameItem);
        m_registerTable->setItem(row, 2, kindItem);
        m_registerTable->setItem(row, 3, countItem);
        m_registerTable->setItem(row, 4, formatItem);
        m_registerTable->setItem(row, 5, valueItem);
        m_registerTable->setItem(row, 6, writableItem);
        m_registerTable->setItem(row, 7, descItem);
    }

    m_updatingTable = false;
}

void MainWindow::applyQuickDebugToRegisters()
{
    RegisterStore::QuickDebugValues values;
    values.instantFlow = static_cast<float>(m_instantFlowSpin->value());
    values.instantVelocity = static_cast<float>(m_instantVelocitySpin->value());
    values.zeroCutoff = static_cast<float>(m_zeroCutoffSpin->value());
    values.language = static_cast<quint16>(m_languageCombo->currentIndex());
    values.sensitivity = static_cast<quint16>(m_sensitivityCombo->currentIndex());
    values.pipeOuterDiameter = static_cast<float>(m_pipeOuterDiameterSpin->value());
    values.pipeWallThickness = static_cast<float>(m_pipeWallThicknessSpin->value());
    values.pipeMaterial = static_cast<quint16>(m_pipeMaterialCombo->currentIndex());
    values.outputMode = static_cast<quint16>(m_outputModeCombo->currentIndex());
    values.pulseEquivalent = static_cast<quint16>(m_pulseEquivalentCombo->currentIndex());
    values.analogUpper = static_cast<float>(m_analogUpperSpin->value());
    values.analogLower = static_cast<float>(m_analogLowerSpin->value());
    values.alarmUpper = static_cast<float>(m_alarmUpperSpin->value());
    values.alarmLower = static_cast<float>(m_alarmLowerSpin->value());
    values.fixedErrorCompensation = static_cast<float>(m_fixedErrorCompSpin->value());
    m_store.applyQuickDebugValues(values);
    appendLog(QStringLiteral("已将快速调试区参数应用到寄存器。"));
}

void MainWindow::loadQuickDebugFromRegisters()
{
    refreshQuickDebugControls(m_store.quickDebugValues());
}

void MainWindow::onRegisterCellChanged(int row, int column)
{
    if (m_updatingTable || column != 5) {
        return;
    }
    const auto *addressItem = m_registerTable->item(row, 0);
    const auto *valueItem = m_registerTable->item(row, 5);
    if (!addressItem || !valueItem) {
        return;
    }

    const uint16_t address = static_cast<uint16_t>(addressItem->text().toUShort());
    QString error;
    if (!m_store.setDisplayValue(address, valueItem->text(), &error)) {
        QMessageBox::warning(this, QStringLiteral("修改失败"), error);
        refreshRegisterTable();
    }
}

void MainWindow::saveSnapshot()
{
    const QString filePath = QFileDialog::getSaveFileName(this, QStringLiteral("保存寄存器快照"), QString(), QStringLiteral("JSON 文件 (*.json)"));
    if (filePath.isEmpty()) {
        return;
    }
    QString error;
    if (!m_store.saveSnapshot(filePath, &error)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return;
    }
    appendLog(QStringLiteral("已保存快照：%1").arg(filePath));
}

void MainWindow::loadSnapshot()
{
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("加载寄存器快照"), QString(), QStringLiteral("JSON 文件 (*.json)"));
    if (filePath.isEmpty()) {
        return;
    }
    QString error;
    if (!m_store.loadSnapshot(filePath, &error)) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), error);
        return;
    }
    appendLog(QStringLiteral("已加载快照：%1").arg(filePath));
}

void MainWindow::appendLog(const QString &message)
{
    m_logEdit->appendPlainText(message);
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("RUF-Q Modbus RTU 串口从站模拟器"));
    resize(1400, 900);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(createSerialConfigGroup(), 1);
    topLayout->addWidget(createQuickDebugGroup(), 2);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(createDataGroup(), 3);
    mainLayout->addWidget(createLogGroup(), 2);

    setCentralWidget(central);

    auto *toolBar = addToolBar(QStringLiteral("快捷操作"));
    toolBar->addAction(QStringLiteral("刷新串口"), this, &MainWindow::refreshPorts);
    toolBar->addAction(QStringLiteral("加载默认模拟数据"), &m_store, &RegisterStore::loadDefaultSimulationData);
    toolBar->addAction(QStringLiteral("随机变化瞬时值"), &m_store, &RegisterStore::randomizeInstantValues);
    toolBar->addAction(QStringLiteral("恢复出厂默认值"), &m_store, &RegisterStore::restoreFactoryDefaults);
    toolBar->addAction(QStringLiteral("保存快照"), this, &MainWindow::saveSnapshot);
    toolBar->addAction(QStringLiteral("加载快照"), this, &MainWindow::loadSnapshot);

    statusBar()->showMessage(QStringLiteral("就绪"));
}

QWidget *MainWindow::createSerialConfigGroup()
{
    auto *group = new QGroupBox(QStringLiteral("A. 串口配置"));
    auto *layout = new QFormLayout(group);

    m_portCombo = new QComboBox;
    m_baudCombo = createEnumCombo({QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("115200")});
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItem(QStringLiteral("5"), QSerialPort::Data5);
    m_dataBitsCombo->addItem(QStringLiteral("6"), QSerialPort::Data6);
    m_dataBitsCombo->addItem(QStringLiteral("7"), QSerialPort::Data7);
    m_dataBitsCombo->addItem(QStringLiteral("8"), QSerialPort::Data8);
    m_dataBitsCombo->setCurrentIndex(3);

    m_parityCombo = new QComboBox;
    m_parityCombo->addItem(QStringLiteral("None"), QSerialPort::NoParity);
    m_parityCombo->addItem(QStringLiteral("Even"), QSerialPort::EvenParity);
    m_parityCombo->addItem(QStringLiteral("Odd"), QSerialPort::OddParity);

    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    m_stopBitsCombo->addItem(QStringLiteral("2"), QSerialPort::TwoStop);

    m_slaveIdSpin = new QSpinBox;
    m_slaveIdSpin->setRange(1, 247);
    m_slaveIdSpin->setValue(1);

    m_togglePortButton = new QPushButton(QStringLiteral("打开串口"));
    connect(m_togglePortButton, &QPushButton::clicked, this, &MainWindow::toggleSerialPort);

    auto *refreshButton = new QPushButton(QStringLiteral("扫描 COM"));
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);

    m_statusLabel = new QLabel(QStringLiteral("未打开"));

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_togglePortButton);
    buttonLayout->addWidget(refreshButton);

    layout->addRow(QStringLiteral("串口号"), m_portCombo);
    layout->addRow(QStringLiteral("波特率"), m_baudCombo);
    layout->addRow(QStringLiteral("数据位"), m_dataBitsCombo);
    layout->addRow(QStringLiteral("校验位"), m_parityCombo);
    layout->addRow(QStringLiteral("停止位"), m_stopBitsCombo);
    layout->addRow(QStringLiteral("从站地址"), m_slaveIdSpin);
    layout->addRow(QStringLiteral("操作"), buttonLayout);
    layout->addRow(QStringLiteral("通信状态"), m_statusLabel);

    return group;
}

QWidget *MainWindow::createDataGroup()
{
    auto *group = new QGroupBox(QStringLiteral("B. 设备数据区"));
    auto *layout = new QVBoxLayout(group);

    m_registerTable = new QTableWidget;
    m_registerTable->setColumnCount(8);
    m_registerTable->setHorizontalHeaderLabels({QStringLiteral("地址"), QStringLiteral("名称"), QStringLiteral("类型"),
                                                QStringLiteral("寄存器数量"), QStringLiteral("数据格式"), QStringLiteral("当前值"),
                                                QStringLiteral("可写性"), QStringLiteral("说明")});
    m_registerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_registerTable->verticalHeader()->setVisible(false);
    connect(m_registerTable, &QTableWidget::cellChanged, this, &MainWindow::onRegisterCellChanged);

    layout->addWidget(m_registerTable);
    return group;
}

QWidget *MainWindow::createQuickDebugGroup()
{
    auto *group = new QGroupBox(QStringLiteral("C. 快速调试区"));
    auto *layout = new QGridLayout(group);

    m_instantFlowSpin = createFloatSpin(-100000.0, 100000.0);
    m_instantVelocitySpin = createFloatSpin(-1000.0, 1000.0);
    m_zeroCutoffSpin = createFloatSpin(-1000.0, 1000.0);
    m_languageCombo = createEnumCombo({QStringLiteral("简体中文"), QStringLiteral("English")});
    m_sensitivityCombo = createEnumCombo({QStringLiteral("低"), QStringLiteral("中"), QStringLiteral("高")});
    m_pipeOuterDiameterSpin = createFloatSpin(0.0, 5000.0);
    m_pipeWallThicknessSpin = createFloatSpin(0.0, 100.0);
    m_pipeMaterialCombo = createEnumCombo({QStringLiteral("PVC"), QStringLiteral("金属"), QStringLiteral("合金")});
    m_outputModeCombo = createEnumCombo({QStringLiteral("4-20mA"), QStringLiteral("PNP脉冲"), QStringLiteral("NPN脉冲"), QStringLiteral("报警输出")});
    m_pulseEquivalentCombo = createEnumCombo({QStringLiteral("0.1mL"), QStringLiteral("1mL"), QStringLiteral("10mL"), QStringLiteral("100mL"),
                                              QStringLiteral("1L"), QStringLiteral("10L"), QStringLiteral("100L"), QStringLiteral("1000L")});
    m_analogUpperSpin = createFloatSpin(-100000.0, 100000.0);
    m_analogLowerSpin = createFloatSpin(-100000.0, 100000.0);
    m_alarmUpperSpin = createFloatSpin(-100000.0, 100000.0);
    m_alarmLowerSpin = createFloatSpin(-100000.0, 100000.0);
    m_fixedErrorCompSpin = createFloatSpin(-10000.0, 10000.0);

    int row = 0;
    layout->addWidget(new QLabel(QStringLiteral("瞬时流量")), row, 0); layout->addWidget(m_instantFlowSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("瞬时流速")), row, 0); layout->addWidget(m_instantVelocitySpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("零切下限")), row, 0); layout->addWidget(m_zeroCutoffSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("语言")), row, 0); layout->addWidget(m_languageCombo, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("灵敏度")), row, 0); layout->addWidget(m_sensitivityCombo, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("管道外径(mm)")), row, 0); layout->addWidget(m_pipeOuterDiameterSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("管道壁厚(mm)")), row, 0); layout->addWidget(m_pipeWallThicknessSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("管道材质")), row, 0); layout->addWidget(m_pipeMaterialCombo, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("输出模式")), row, 0); layout->addWidget(m_outputModeCombo, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("脉冲当量")), row, 0); layout->addWidget(m_pulseEquivalentCombo, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("模拟上限")), row, 0); layout->addWidget(m_analogUpperSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("模拟下限")), row, 0); layout->addWidget(m_analogLowerSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("报警上限")), row, 0); layout->addWidget(m_alarmUpperSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("报警下限")), row, 0); layout->addWidget(m_alarmLowerSpin, row++, 1);
    layout->addWidget(new QLabel(QStringLiteral("固定误差补偿")), row, 0); layout->addWidget(m_fixedErrorCompSpin, row++, 1);

    auto *buttonLayout = new QHBoxLayout;
    auto *applyButton = new QPushButton(QStringLiteral("应用到寄存器"));
    auto *refreshButton = new QPushButton(QStringLiteral("从寄存器刷新显示"));
    connect(applyButton, &QPushButton::clicked, this, &MainWindow::applyQuickDebugToRegisters);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::loadQuickDebugFromRegisters);
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(refreshButton);

    layout->addLayout(buttonLayout, row, 0, 1, 2);
    return group;
}

QWidget *MainWindow::createLogGroup()
{
    auto *group = new QGroupBox(QStringLiteral("D. 通信日志区"));
    auto *layout = new QVBoxLayout(group);
    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    layout->addWidget(m_logEdit);
    return group;
}

void MainWindow::refreshQuickDebugControls(const RegisterStore::QuickDebugValues &values)
{
    m_instantFlowSpin->setValue(values.instantFlow);
    m_instantVelocitySpin->setValue(values.instantVelocity);
    m_zeroCutoffSpin->setValue(values.zeroCutoff);
    m_languageCombo->setCurrentIndex(values.language);
    m_sensitivityCombo->setCurrentIndex(values.sensitivity);
    m_pipeOuterDiameterSpin->setValue(values.pipeOuterDiameter);
    m_pipeWallThicknessSpin->setValue(values.pipeWallThickness);
    m_pipeMaterialCombo->setCurrentIndex(values.pipeMaterial);
    m_outputModeCombo->setCurrentIndex(values.outputMode);
    m_pulseEquivalentCombo->setCurrentIndex(values.pulseEquivalent);
    m_analogUpperSpin->setValue(values.analogUpper);
    m_analogLowerSpin->setValue(values.analogLower);
    m_alarmUpperSpin->setValue(values.alarmUpper);
    m_alarmLowerSpin->setValue(values.alarmLower);
    m_fixedErrorCompSpin->setValue(values.fixedErrorCompensation);
}

void MainWindow::updateStatusLabel(const QString &message, bool opened)
{
    m_statusLabel->setText(message);
    m_togglePortButton->setText(opened ? QStringLiteral("关闭串口") : QStringLiteral("打开串口"));
    statusBar()->showMessage(message, 3000);
}
