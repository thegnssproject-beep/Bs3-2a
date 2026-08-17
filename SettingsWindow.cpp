#include "SettingsWindow.h"
#include "PostProcessor.h"
#include "SDRPlotWindow.h"
#include "DataProbeDialog.h"
#include <QFileDialog>
#include <QMessageBox>

SettingsWindow::SettingsWindow(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setWindowTitle("BDS-3 B2a SDR Settings");
    resize(540, 530);
}

void SettingsWindow::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    tabWidget = new QTabWidget(this);

    tabWidget->addTab(createProcessingTab(), "Processing & File");
    tabWidget->addTab(createAcquisitionTab(), "Acquisition & Code");
    tabWidget->addTab(createTrackingTab(), "Tracking & Navigation");

    mainLayout->addWidget(tabWidget);

    btnStart = new QPushButton("Start Processing SDR");
    btnStart->setFixedHeight(35);
    btnStart->setStyleSheet("font-weight: bold; font-size: 13px; background-color: #2b579a; color: white; border-radius: 4px;");

    txtLog = new QTextEdit();
    txtLog->setReadOnly(true);
    txtLog->setMaximumHeight(90);
    txtLog->setPlaceholderText("SDR Status Log Output...");

    mainLayout->addWidget(btnStart);
    mainLayout->addWidget(txtLog);

    connect(btnStart, &QPushButton::clicked, this, &SettingsWindow::onStartClicked);
}

QWidget* SettingsWindow::createProcessingTab()
{
    QWidget* tab = new QWidget();
    QFormLayout* layout = new QFormLayout(tab);

    QHBoxLayout* fileLayout = new QHBoxLayout();
    editFileName = new QLineEdit();
    editFileName->setPlaceholderText("Select or enter .bin signal file...");
    btnBrowse = new QPushButton("Browse...");

    QPushButton* btnProbe = new QPushButton("Probe Data...");
    btnProbe->setStyleSheet("font-weight: bold; background-color: #4a6572; color: white; border-radius: 3px; padding: 4px 8px;");

    fileLayout->addWidget(editFileName);
    fileLayout->addWidget(btnBrowse);
    fileLayout->addWidget(btnProbe);

    connect(btnBrowse, &QPushButton::clicked, this, &SettingsWindow::browseFile);
    connect(btnProbe, &QPushButton::clicked, this, [this]() {
        Settings s = getSettings();
        if (s.fileName.isEmpty()) {
            QMessageBox::warning(this, "Missing File", "Please select a .bin signal file first to inspect.");
            return;
        }
        DataProbeDialog::inspect(s, this);
        });

    comboDataType = new QComboBox();
    comboDataType->addItem("schar");

    comboFileType = new QComboBox();
    comboFileType->addItem("1 - Real Samples");
    comboFileType->addItem("2 - IQ Samples");

    spinMsToProcess = new QSpinBox();
    spinMsToProcess->setRange(1, 999999);
    spinMsToProcess->setValue(49000);

    spinChannels = new QSpinBox();
    spinChannels->setRange(1, 32);
    spinChannels->setValue(12);

    editSkipBytes = new QLineEdit("0");
    editIF = new QLineEdit("13.55e6");
    editSamplingFreq = new QLineEdit("99.375e6");

    layout->addRow("File Name:", fileLayout);
    layout->addRow("Data Type:", comboDataType);
    layout->addRow("File Type:", comboFileType);
    layout->addRow("MS to Process:", spinMsToProcess);
    layout->addRow("Channels:", spinChannels);
    layout->addRow("Skip Bytes:", editSkipBytes);
    layout->addRow("Intermediate Freq (Hz):", editIF);
    layout->addRow("Sampling Freq (Hz):", editSamplingFreq);

    return tab;
}

QWidget* SettingsWindow::createAcquisitionTab()
{
    QWidget* tab = new QWidget();
    QFormLayout* layout = new QFormLayout(tab);

    checkSkipAcq = new QCheckBox("Skip Acquisition");
    editSatList = new QLineEdit("19 20");

    spinSearchBand = new QDoubleSpinBox();
    spinSearchBand->setRange(0, 50000);
    spinSearchBand->setValue(5000.0);

    spinAcqThreshold = new QDoubleSpinBox();
    spinAcqThreshold->setRange(0.1, 10.0);
    spinAcqThreshold->setSingleStep(0.1);
    spinAcqThreshold->setValue(1.1);

    spinAcqStep = new QDoubleSpinBox();
    spinAcqStep->setRange(10, 2000);
    spinAcqStep->setValue(400.0);

    spinFineNoncoh = new QSpinBox();
    spinFineNoncoh->setValue(15);

    checkResampling = new QCheckBox("Enable Resampling");
    editCodeLength = new QLineEdit("10230");
    editCodeFreq = new QLineEdit("10.23e6");

    layout->addRow("", checkSkipAcq);
    layout->addRow("Satellites (PRNs):", editSatList);
    layout->addRow("Search Band (Hz):", spinSearchBand);
    layout->addRow("Acquisition Threshold:", spinAcqThreshold);
    layout->addRow("Frequency Step (Hz):", spinAcqStep);
    layout->addRow("Fine Non-coherent:", spinFineNoncoh);
    layout->addRow("", checkResampling);
    layout->addRow("Code Length:", editCodeLength);
    layout->addRow("Code Freq Basis:", editCodeFreq);

    return tab;
}

QWidget* SettingsWindow::createTrackingTab()
{
    QWidget* tab = new QWidget();
    QFormLayout* layout = new QFormLayout(tab);

    checkPilotTrack = new QCheckBox("Enable Pilot Tracking");
    checkPilotTrack->setChecked(true);

    spinDllDamping = new QDoubleSpinBox();
    spinDllDamping->setValue(0.7);

    spinDllNoise = new QDoubleSpinBox();
    spinDllNoise->setValue(2.0);

    spinPllDamping = new QDoubleSpinBox();
    spinPllDamping->setValue(0.7);

    spinPllNoise = new QDoubleSpinBox();
    spinPllNoise->setValue(20.0);

    spinNavPeriod = new QSpinBox();
    spinNavPeriod->setRange(100, 5000);
    spinNavPeriod->setValue(500);

    spinElevMask = new QSpinBox();
    spinElevMask->setRange(0, 90);
    spinElevMask->setValue(5);

    checkTropo = new QCheckBox("Use Tropospheric Correction");
    checkTropo->setChecked(true);

    layout->addRow("", checkPilotTrack);
    layout->addRow("DLL Damping Ratio:", spinDllDamping);
    layout->addRow("DLL Noise Bandwidth (Hz):", spinDllNoise);
    layout->addRow("PLL Damping Ratio:", spinPllDamping);
    layout->addRow("PLL Noise Bandwidth (Hz):", spinPllNoise);
    layout->addRow("Nav Period (ms):", spinNavPeriod);
    layout->addRow("Elevation Mask (°):", spinElevMask);
    layout->addRow("", checkTropo);

    return tab;
}

void SettingsWindow::browseFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select Signal File", "", "Binary Files (*.bin);;All Files (*)");
    if (!filePath.isEmpty()) {
        editFileName->setText(filePath);
    }
}

void SettingsWindow::onStartClicked()
{
    Settings s = getSettings();
    if (s.fileName.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing File", "Please select or enter a valid raw binary signal file (.bin) first.");
        return;
    }

    txtLog->append("[INFO] Configuration validated successfully.");
    txtLog->append("[INFO] Launching SDR post-processor...");

    emit startProcessingRequested(s);

    btnStart->setEnabled(false);
    btnBrowse->setEnabled(false);

    SDRPipelineResults results;
    bool success = PostProcessor::runPipeline(s, results);

    if (success) {
        txtLog->append("[INFO] Post-processing completed. Opening Plotter...");

        SDRPlotWindow* plotter = new SDRPlotWindow();
        plotter->setAttribute(Qt::WA_DeleteOnClose);

        plotter->plotAcquisitionResults(results.acqResults);

        // Find the first active tracked satellite channel (PRN > 0)
        bool channelPlotted = false;
        for (const auto& ch : results.trackResults) {
            if (ch.PRN > 0 && !ch.I_P.empty()) {
                plotter->plotTrackingResults(ch);
                channelPlotted = true;
                break;
            }
        }
        if (!channelPlotted && !results.trackResults.empty()) {
            plotter->plotTrackingResults(results.trackResults[0]);
        }

        plotter->plotNavigationResults(results.navSolutions);

        plotter->show();
        plotter->raise();
        plotter->activateWindow();
    }
    else {
        txtLog->append("[ERROR] Pipeline execution failed to open or process the raw signal file.");
    }

    btnStart->setEnabled(true);
    btnBrowse->setEnabled(true);
}

Settings SettingsWindow::getSettings() const
{
    Settings s;
    s.fileName = editFileName->text().trimmed();
    s.dataType = comboDataType->currentText();
    s.fileType = (comboFileType->currentIndex() == 0) ? 1 : 2;
    s.msToProcess = spinMsToProcess->value();
    s.numberOfChannels = spinChannels->value();
    s.skipNumberOfBytes = editSkipBytes->text().toLongLong();
    s.IF = editIF->text().toDouble();
    s.samplingFreq = editSamplingFreq->text().toDouble();

    s.skipAcquisition = checkSkipAcq->isChecked();
    s.acqSatelliteList = editSatList->text();
    s.acqSearchBand = spinSearchBand->value();
    s.acqThreshold = spinAcqThreshold->value();
    s.acqStep = spinAcqStep->value();
    s.fineNoncoh = spinFineNoncoh->value();
    s.resamplingflag = checkResampling->isChecked();
    s.codeLength = editCodeLength->text().toInt();
    s.codeFreqBasis = editCodeFreq->text().toDouble();

    s.pilotTRKflag = checkPilotTrack->isChecked();
    s.dllDampingRatio = spinDllDamping->value();
    s.dllNoiseBandwidth = spinDllNoise->value();
    s.pllDampingRatio = spinPllDamping->value();
    s.pllNoiseBandwidth = spinPllNoise->value();
    s.navSolPeriod = spinNavPeriod->value();
    s.elevationMask = spinElevMask->value();
    s.useTropCorr = checkTropo->isChecked();

    return s;
}