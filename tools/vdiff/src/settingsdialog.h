#pragma once

#include <QDialog>

namespace Ui {
class SettingsDialog;
}

class Settings;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(Settings *settings, QWidget *parent = nullptr);
    ~SettingsDialog();

private:
    void loadSettings();

private slots:
    void on_buttonBox_accepted();
    void on_btnSelectBatik_clicked();
    void on_btnSelectJSVG_clicked();
    void on_btnSelectSVGSalamander_clicked();
    void on_btnSelectEchoSVG_clicked();
    void on_btnSelectTest_clicked();
    void prepareTestsPathWidgets();

private:
    Ui::SettingsDialog * const ui;
    Settings * const m_settings;
};
