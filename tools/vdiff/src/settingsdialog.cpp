#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>

#include "settings.h"

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_settings(settings)
{
    ui->setupUi(this);

    loadSettings();
    setMinimumWidth(600);
    adjustSize();

    auto suiteGroup = new QButtonGroup(this);
    suiteGroup->addButton(ui->rBtnSuiteResvg);
    suiteGroup->addButton(ui->rBtnSuiteCustom);
    connect(suiteGroup, SIGNAL(buttonToggled(QAbstractButton*,bool)),
            this, SLOT(prepareTestsPathWidgets()));

    ui->buttonBox->setFocus();
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::loadSettings()
{
    ui->rBtnSuiteCustom->setChecked(m_settings->testSuite == TestSuite::Custom);
    ui->rBtnRelease->setChecked(m_settings->buildType == BuildType::Release);
    ui->lineEditTestsPath->setText(m_settings->customTestsPath);
    ui->lineEditResvg->setText(m_settings->resvgDir);

    ui->chBoxUseBatik->setChecked(m_settings->useBatik);
    ui->lineEditBatik->setText(m_settings->batikPath);

    ui->chBoxUseJSVG->setChecked(m_settings->useJSVG);
    ui->lineEditJSVG->setText(m_settings->jsvgPath);

    ui->chBoxUseSVGSalamander->setChecked(m_settings->useSVGSalamander);
    ui->lineEditSVGSalamander->setText(m_settings->svgsalamanderPath);

    prepareTestsPathWidgets();
}

void SettingsDialog::prepareTestsPathWidgets()
{
    const auto isCustom = ui->rBtnSuiteCustom->isChecked();
    ui->lineEditTestsPath->setVisible(isCustom);
    ui->btnSelectTest->setVisible(isCustom);
    //ui->chBoxUseChrome->setEnabled(!isCustom);

    if (isCustom) {
      //  ui->chBoxUseChrome->setChecked(true);
    }
}

void SettingsDialog::on_buttonBox_accepted()
{
    auto suite = TestSuite::Own;
    if (ui->rBtnSuiteCustom->isChecked()) {
        suite = TestSuite::Custom;
    }
    m_settings->testSuite = suite;
    m_settings->customTestsPath = ui->lineEditTestsPath->text();

    m_settings->buildType = ui->rBtnRelease->isChecked()
                    ? BuildType::Release
                    : BuildType::Debug;

    m_settings->useBatik = ui->chBoxUseBatik->isChecked();
    m_settings->useJSVG = ui->chBoxUseJSVG->isChecked();
    m_settings->useSVGSalamander = ui->chBoxUseSVGSalamander->isChecked();

    m_settings->batikPath = ui->lineEditBatik->text();
    m_settings->jsvgPath = ui->lineEditJSVG->text();
    m_settings->svgsalamanderPath = ui->lineEditSVGSalamander->text();

    m_settings->save();
}

void SettingsDialog::on_btnSelectTest_clicked()
{
    const auto path = QFileDialog::getExistingDirectory(this, "Custom tests path",
                                                        ui->lineEditTestsPath->text());
    if (!path.isEmpty()) {
        ui->lineEditTestsPath->setText(path);
    }
}

void SettingsDialog::on_btnSelectResvg_clicked()
{
    const auto path = QFileDialog::getExistingDirectory(this, "resvg source path");
    if (!path.isEmpty()) {
        ui->lineEditResvg->setText(path);
    }
}

void SettingsDialog::on_btnSelectBatik_clicked()
{
    const auto path = QFileDialog::getOpenFileName(this, "batik-rasterizer exe path");
    if (!path.isEmpty()) {
        ui->lineEditBatik->setText(path);
    }
}

void SettingsDialog::on_btnSelectJSVG_clicked()
{
    const auto path = QFileDialog::getOpenFileName(this, "jsvg-rasterizer exe path");
    if (!path.isEmpty()) {
        ui->lineEditJSVG->setText(path);
    }
}

void SettingsDialog::on_btnSelectSVGSalamander_clicked()
{
    const auto path = QFileDialog::getOpenFileName(this, "svgsalamander-rasterizer exe path");
    if (!path.isEmpty()) {
        ui->lineEditSVGSalamander->setText(path);
    }
}
