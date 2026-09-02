#include "SettingsWindow.h"
#include "PostProcessor.h"
#include "SDRPlotWindow.h"
#include "ProbeDataWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QThread>
#include <QMetaType>
#include <QCoreApplication>
#include <QStringList>
#include <fstream>
#include <vector>
#include <complex>

SettingsWindow::SettingsWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    setWindowTitle("BDS-3 B2a SDR GNSS Receiver & Post-Processor");
    resize(740, 700);

    // Register custom payload types so they can flow over queued
    // (cross-thread) signal connections between the worker and this window.
    qRegisterMetaType<AcqResults>("AcqResults");
    qRegisterMetaType<std::vector<ChannelTrackResult>>("std::vector<ChannelTrackResult>");
    qRegisterMetaType<NavSolutions>("NavSolutions");

    loadSettingsToUI();
}

void SettingsWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* rootLayout = new QVBoxLayout(centralWidget);

    // ==========================================
    // 1. Settings Parameter Panel
    // ==========================================
    QGroupBox* grpSettings = new QGroupBox("Receiver & Processing Parameters", this);
    QFormLayout* formLayout = new QFormLayout(grpSettings);

    QHBoxLayout* fileLayout = new QHBoxLayout();
    editFileName = new QLineEdit(this);
    editFileName->setPlaceholderText("Select raw BDS-3 IF data file (.bin, .dat)...");
    btnBrowse = new QPushButton("Browse...", this);
    fileLayout->addWidget(editFileName);
    fileLayout->addWidget(btnBrowse);
    formLayout->addRow("Signal File:", fileLayout);

    editSatelliteList = new QLineEdit(this);
    editSatelliteList->setPlaceholderText("e.g. 1-64, 19:56, or 19 20 37 40 43");
    formLayout->addRow("Satellite PRN(s) / Range:", editSatelliteList);

    QLabel* lblMode = new QLabel("Channel Selection Mode:", this);
    comboSatMode = new QComboBox(this);
    comboSatMode->addItem("All Acquired (Above Threshold)", 0);
    comboSatMode->addItem("Top Strongest Peaks (Limit to Max Channels)", 3);
    comboSatMode->addItem("Strict Specified PRN List", 1);
    formLayout->addRow(lblMode, comboSatMode);

    spinAcqThreshold = new QDoubleSpinBox(this);
    spinAcqThreshold->setRange(0.5, 10.0);
    spinAcqThreshold->setSingleStep(0.05);
    spinAcqThreshold->setDecimals(2);
    formLayout->addRow("Acquisition Threshold:", spinAcqThreshold);

    spinChannels = new QSpinBox(this);
    spinChannels->setRange(1, 64);
    formLayout->addRow("Max Channels:", spinChannels);

    spinMsToProcess = new QSpinBox(this);
    spinMsToProcess->setRange(100, 600000);
    spinMsToProcess->setSingleStep(1000);
    spinMsToProcess->setSuffix(" ms");
    formLayout->addRow("Duration to Process:", spinMsToProcess);

    spinIF = new QDoubleSpinBox(this);
    spinIF->setRange(0.0, 100.0);
    spinIF->setDecimals(4);
    spinIF->setSuffix(" MHz");
    formLayout->addRow("Intermediate Freq (IF):", spinIF);

    spinSamplingFreq = new QDoubleSpinBox(this);
    spinSamplingFreq->setRange(1.0, 500.0);
    spinSamplingFreq->setDecimals(4);
    spinSamplingFreq->setSuffix(" MHz");
    formLayout->addRow("Sampling Frequency (fs):", spinSamplingFreq);

    rootLayout->addWidget(grpSettings);

    // ==========================================
    // 2. Action Buttons & Progress Bar
    // ==========================================
    QHBoxLayout* actionLayout = new QHBoxLayout();

    btnProbe = new QPushButton("Probe Data (Raw Spectrum)", this);
    btnProbe->setFixedHeight(36);
    btnProbe->setStyleSheet("QPushButton { font-weight: bold; background-color: #555555; color: white; border-radius: 4px; padding: 0 14px; } QPushButton:hover { background-color: #6E6E6E; }");

    btnRun = new QPushButton("Run SDR Post-Processor", this);
    btnRun->setFixedHeight(36);
    btnRun->setStyleSheet("QPushButton { font-weight: bold; background-color: #0078D7; color: white; border-radius: 4px; padding: 0 18px; } QPushButton:hover { background-color: #1E90FF; }");

    btnCancel = new QPushButton("Cancel", this);
    btnCancel->setFixedHeight(36);
    btnCancel->setStyleSheet("QPushButton { font-weight: bold; background-color: #C0504D; color: white; border-radius: 4px; padding: 0 18px; } QPushButton:hover { background-color: #D75F5C; }");
    btnCancel->setEnabled(false);
    btnCancel->setVisible(false);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setVisible(false);
    progressBar->setFixedHeight(24);

    actionLayout->addWidget(btnProbe);
    actionLayout->addWidget(btnRun);
    actionLayout->addWidget(btnCancel);
    actionLayout->addWidget(progressBar);
    rootLayout->addLayout(actionLayout);

    // ==========================================
    // 3. Live Console Terminal Output
    // ==========================================
    QGroupBox* grpConsole = new QGroupBox("Processing Log & Console Output", this);
    QVBoxLayout* consoleLayout = new QVBoxLayout(grpConsole);
    consoleOutput = new QTextEdit(this);
    consoleOutput->setReadOnly(true);
    consoleOutput->setStyleSheet("QTextEdit { background-color: #1E1E1E; color: #DCDCDC; font-family: Consolas, monospace; font-size: 11px; }");
    consoleLayout->addWidget(consoleOutput);
    rootLayout->addWidget(grpConsole);

    connect(btnBrowse, &QPushButton::clicked, this, &SettingsWindow::onBrowseFile);
    connect(btnProbe, &QPushButton::clicked, this, &SettingsWindow::onProbeDataClicked);
    connect(btnRun, &QPushButton::clicked, this, &SettingsWindow::onRunSDRClicked);
    connect(btnCancel, &QPushButton::clicked, this, &SettingsWindow::onCancelSDRClicked);
}

void SettingsWindow::loadSettingsToUI()
{
    if (editFileName) editFileName->setText(settings.fileName.isEmpty() ? "C:/Yousuf_B2a/dump1_ch3_1.bin" : settings.fileName);
    if (editSatelliteList) editSatelliteList->setText(settings.targetSatList.isEmpty() ? "19:56" : settings.targetSatList);
    if (spinAcqThreshold) spinAcqThreshold->setValue(settings.acqThreshold > 0.0 ? settings.acqThreshold : 1.50);
    if (spinChannels) spinChannels->setValue(settings.numberOfChannels > 0 ? settings.numberOfChannels : 12);
    if (spinMsToProcess) spinMsToProcess->setValue(static_cast<int>(settings.msToProcess > 0 ? settings.msToProcess : 49000));
    if (spinIF) spinIF->setValue(settings.IF > 0.0 ? settings.IF / 1e6 : 13.55);
    if (spinSamplingFreq) spinSamplingFreq->setValue(settings.samplingFreq > 0.0 ? settings.samplingFreq / 1e6 : 99.375);

    if (comboSatMode) {
        int index = comboSatMode->findData(settings.satSelectMode);
        if (index != -1) comboSatMode->setCurrentIndex(index);
    }
}

void SettingsWindow::saveUIToSettings()
{
    if (editFileName) settings.fileName = editFileName->text().trimmed();
    if (editSatelliteList) {
        QString listText = editSatelliteList->text().trimmed();
        settings.targetSatList = listText;
        settings.acqSatelliteList = listText;
    }
    if (spinAcqThreshold) settings.acqThreshold = spinAcqThreshold->value();
    if (spinChannels) settings.numberOfChannels = spinChannels->value();
    if (spinMsToProcess) settings.msToProcess = spinMsToProcess->value();
    if (spinIF) settings.IF = spinIF->value() * 1e6;
    if (spinSamplingFreq) settings.samplingFreq = spinSamplingFreq->value() * 1e6;

    if (comboSatMode) settings.satSelectMode = comboSatMode->currentData().toInt();
    if (spinChannels) settings.maxChannels = spinChannels->value();
}

void SettingsWindow::logMessage(const QString& msg)
{
    if (consoleOutput) {
        consoleOutput->append(msg);
        consoleOutput->ensureCursorVisible();
        QCoreApplication::processEvents();
    }
}

void SettingsWindow::onBrowseFile()
{
    QString f = QFileDialog::getOpenFileName(this, "Select Raw IF File", "", "Binary Files (*.bin *.dat *.raw);;All Files (*.*)");
    if (!f.isEmpty() && editFileName) {
        editFileName->setText(f);
    }
}

void SettingsWindow::onProbeDataClicked()
{
    saveUIToSettings();
    if (settings.fileName.isEmpty()) {
        QMessageBox::warning(this, "Missing File", "Please select a valid raw IF file.");
        return;
    }

    logMessage("Probing data (" + settings.fileName + ")...");

    std::ifstream fid(settings.fileName.toStdString(), std::ios::binary);
    if (!fid.is_open()) {
        logMessage("[ERROR] Could not open signal file for probing.");
        return;
    }

    int bytesPerSample = (settings.fileType == 1) ? 1 : 2;
    int samplesToRead = 65536;
    std::vector<int8_t> buffer(samplesToRead * bytesPerSample);
    fid.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    fid.close();

    std::vector<std::complex<double>> probeSignal(samplesToRead);
    if (bytesPerSample == 1) {
        for (int i = 0; i < samplesToRead; ++i) {
            probeSignal[i] = std::complex<double>(static_cast<double>(buffer[i]), 0.0);
        }
    }
    else {
        for (int i = 0; i < samplesToRead; ++i) {
            probeSignal[i] = std::complex<double>(static_cast<double>(buffer[2 * i]), static_cast<double>(buffer[2 * i + 1]));
        }
    }

    logMessage("  Raw IF data plotted in independent Probe Window.");

    ProbeDataWindow* probeWin = new ProbeDataWindow(this);
    probeWin->setAttribute(Qt::WA_DeleteOnClose);
    probeWin->plotProbeData(probeSignal, settings.samplingFreq, settings.fileType);
    probeWin->show();
}

void SettingsWindow::onRunSDRClicked()
{
    saveUIToSettings();

    if (settings.fileName.isEmpty()) {
        QMessageBox::warning(this, "Missing File", "Please select a valid raw IF binary file first.");
        return;
    }

    startPipeline();
}

void SettingsWindow::onCancelSDRClicked()
{
    if (settings.cancelRequested) {
        settings.cancelRequested->store(true);
        logMessage("[INFO] Cancel requested... finishing current step.");
        btnCancel->setEnabled(false);
    }
}

void SettingsWindow::onWorkerLog(const QString& msg)
{
    logMessage(msg);
}

void SettingsWindow::onWorkerProgress(int percent)
{
    if (progressBar) {
        progressBar->setValue(percent);
        progressBar->setFormat(QString("Processing... %p%"));
    }
}

SDRPlotWindow* SettingsWindow::ensurePlotWindow()
{
    if (!m_plotWindow) {
        // No parent: the plot window is an independent top-level window. It
        // must not be a child of this settings window, otherwise minimizing or
        // closing the settings window here would minimize/close the plots too.
        // WA_DeleteOnClose lets the user close it freely, and the destroyed
        // connection makes sure a closed window is recreated on the next run.
        m_plotWindow = new SDRPlotWindow();
        m_plotWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_plotWindow, &QObject::destroyed, this, [this]() {
            m_plotWindow = nullptr;
        });
    }
    return m_plotWindow;
}

void SettingsWindow::onAcquisitionReady(const AcqResults& acqResults, const QVector<int>& trackedPrns)
{
    logMessage(QString("[ACQ] tracked PRNs (%1): %2")
        .arg(trackedPrns.size())
        .arg([&]() { QStringList l; for (int p : trackedPrns) l << QString::number(p); return l.join(", "); }()));

    SDRPlotWindow* plotWin = ensurePlotWindow();
    std::vector<int> prns(trackedPrns.begin(), trackedPrns.end());
    plotWin->plotAcquisitionResults(acqResults, settings.acqThreshold, prns, settings.IF);
    plotWin->show();
    plotWin->raise();
    plotWin->activateWindow();
}

void SettingsWindow::onTrackingReady(const std::vector<ChannelTrackResult>& trackResults)
{
    int nchan = 0;
    for (const auto& tr : trackResults) if (tr.PRN > 0 && !tr.I_P.empty()) nchan++;
    logMessage(QString("[TRK] tracked channels=%1 / %2").arg(nchan).arg(trackResults.size()));

    m_lastTrackResults = trackResults; // cache for sky plot fallback

    SDRPlotWindow* plotWin = ensurePlotWindow();
    plotWin->plotTrackingResults(trackResults);
    plotWin->show();
}

void SettingsWindow::onNavigationReady(const NavSolutions& navSolutions)
{
    int nfix = 0;
    double maxEl = 0.0;
    for (size_t i = 0; i < navSolutions.latitude.size(); ++i) {
        if (std::abs(navSolutions.latitude[i]) > 1e-4) nfix++;
    }
    for (const auto& elRow : navSolutions.elevation) {
        for (double e : elRow) if (e > maxEl) maxEl = e;
    }
    logMessage(QString("[NAV] lat.size=%1 fixes=%2 activePrns=%3 maxEl=%4")
        .arg(navSolutions.latitude.size())
        .arg(nfix)
        .arg(navSolutions.activePrns.size())
        .arg(maxEl));

    SDRPlotWindow* plotWin = ensurePlotWindow();
    plotWin->plotNavigationResults(navSolutions, &m_lastTrackResults);
}

void SettingsWindow::onWorkerFinished(bool ok)
{
    // Guard against a stale queued signal from a previous (cancelled) run that
    // fired after a new pipeline started; only the currently-active worker may
    // touch our pointers/UI.
    if (sender() != m_worker) {
        return;
    }

    // The thread/worker are being deleted via deleteLater; just reset our
    // pointers and restore the UI.
    m_worker = nullptr;
    m_sdrThread = nullptr;

    if (btnRun) btnRun->setEnabled(true);
    if (btnProbe) btnProbe->setEnabled(true);
    if (btnCancel) { btnCancel->setEnabled(false); btnCancel->setVisible(false); }
    if (progressBar) progressBar->setVisible(false);

    settings.cancelRequested.reset();

    if (ok) {
        logMessage("[INFO] Processing Complete.");
    }
    else {
        logMessage("[INFO] Processing cancelled or failed.");
    }
}

void SettingsWindow::onThreadFinished()
{
    // The worker thread has fully exited here, so it is now safe to finish a
    // close that was deferred while the pipeline was still running.
    if (m_closeRequested) {
        m_closeRequested = false;
        close();
    }
}

void SettingsWindow::startPipeline()
{
    // A close has been requested; do not start new work, the window is exiting.
    if (m_closeRequested) return;

    // Cancel any previous run first.
    stopPipeline();

    if (consoleOutput) consoleOutput->clear();
    if (btnRun) btnRun->setEnabled(false);
    if (btnProbe) btnProbe->setEnabled(false);
    if (btnCancel) { btnCancel->setEnabled(true); btnCancel->setVisible(true); }
    if (progressBar) { progressBar->setValue(0); progressBar->setVisible(true); }

    settings.cancelRequested = std::make_shared<std::atomic<bool>>(false);

    logMessage("[INFO] Starting BDS-3 B2a SDR Pipeline...");

    m_sdrThread = new QThread(this);
    m_worker = new SDRPipelineWorker(settings); // parentless; owned by thread

    m_worker->moveToThread(m_sdrThread);

    connect(m_sdrThread, &QThread::started, m_worker, &SDRPipelineWorker::process);
    connect(m_worker, &SDRPipelineWorker::logMessage, this, &SettingsWindow::onWorkerLog);
    connect(m_worker, &SDRPipelineWorker::progressChanged, this, &SettingsWindow::onWorkerProgress);
    connect(m_worker, &SDRPipelineWorker::acquisitionReady, this, &SettingsWindow::onAcquisitionReady);
    connect(m_worker, &SDRPipelineWorker::trackingReady, this, &SettingsWindow::onTrackingReady);
    connect(m_worker, &SDRPipelineWorker::navigationReady, this, &SettingsWindow::onNavigationReady);
    connect(m_worker, &SDRPipelineWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &SDRPipelineWorker::finished, m_sdrThread, &QThread::quit);
    connect(m_worker, &SDRPipelineWorker::finished, this, &SettingsWindow::onWorkerFinished);
    connect(m_sdrThread, &QThread::finished, this, &SettingsWindow::onThreadFinished);
    connect(m_sdrThread, &QThread::finished, m_sdrThread, &QObject::deleteLater);

    m_sdrThread->start();
}

void SettingsWindow::stopPipeline()
{
    if (settings.cancelRequested) {
        settings.cancelRequested->store(true);
    }

    if (m_sdrThread && m_sdrThread->isRunning()) {
        // Ask the worker to stop cooperatively; wait a bounded time for it to
        // wrap up its current stage.
        m_sdrThread->quit();
        if (!m_sdrThread->wait(15000)) {
            // Last resort: only reached during shutdown if a stage failed to
            // honour the cancel flag. Terminating the thread prevents the
            // running-QThread destructor from hanging the GUI thread forever.
            m_sdrThread->terminate();
            m_sdrThread->wait(5000);
        }
    }

    if (m_sdrThread) {
        m_sdrThread->deleteLater();
        m_sdrThread = nullptr;
    }
    m_worker = nullptr;

    settings.cancelRequested.reset();
}

SettingsWindow::~SettingsWindow()
{
    stopPipeline();
}

void SettingsWindow::closeEvent(QCloseEvent* event)
{
    if (m_sdrThread && m_sdrThread->isRunning()) {
        // Never destroy a running QThread and never block the GUI thread in
        // wait(). Ask the worker to stop cooperatively and defer the real close
        // to onWorkerFinished (invoked once the thread has actually returned).
        m_closeRequested = true;
        if (settings.cancelRequested) {
            settings.cancelRequested->store(true);
        }
        m_sdrThread->quit();
        if (btnRun) btnRun->setEnabled(false);
        logMessage("[INFO] Closing after processing finishes...");
        event->ignore();
        return;
    }
    event->accept();
}