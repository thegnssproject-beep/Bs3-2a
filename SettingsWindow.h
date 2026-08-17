#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include "Settings.h"

class SettingsWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);
    Settings getSettings() const;

signals:
    void startProcessingRequested(const Settings& settings);

private slots:
    void browseFile();
    void onStartClicked();

private:
    void setupUI();

    QWidget* createProcessingTab();
    QWidget* createAcquisitionTab();
    QWidget* createTrackingTab();

    QTabWidget* tabWidget;

    QLineEdit* editFileName;
    QPushButton* btnBrowse;
    QComboBox* comboDataType;
    QComboBox* comboFileType;
    QSpinBox* spinMsToProcess;
    QSpinBox* spinChannels;
    QLineEdit* editSkipBytes;
    QLineEdit* editIF;
    QLineEdit* editSamplingFreq;

    QCheckBox* checkSkipAcq;
    QLineEdit* editSatList;
    QDoubleSpinBox* spinSearchBand;
    QDoubleSpinBox* spinAcqThreshold;
    QDoubleSpinBox* spinAcqStep;
    QSpinBox* spinFineNoncoh;
    QCheckBox* checkResampling;
    QLineEdit* editCodeLength;
    QLineEdit* editCodeFreq;

    QCheckBox* checkPilotTrack;
    QDoubleSpinBox* spinDllDamping;
    QDoubleSpinBox* spinDllNoise;
    QDoubleSpinBox* spinPllDamping;
    QDoubleSpinBox* spinPllNoise;
    QSpinBox* spinNavPeriod;
    QSpinBox* spinElevMask;
    QCheckBox* checkTropo;

    QPushButton* btnStart;
    QTextEdit* txtLog;
};