#include <QPushButton>

#include "exportdialog.h"
#include "ui_exportdialog.h"

ExportDialog::ExportDialog(const QList<Backend> &backends, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ExportDialog)
{
    ui->setupUi(this);

    ui->chBoxBReference->setEnabled(backends.contains(Backend::Reference));
    ui->chBoxBBatik->setEnabled(backends.contains(Backend::Batik));
    ui->chBoxBJSVG->setEnabled(backends.contains(Backend::JSVG)); 
    ui->chBoxBSVGSalamander->setEnabled(backends.contains(Backend::SVGSalamander));
    ui->chBoxBEchoSVG->setEnabled(backends.contains(Backend::EchoSVG));

    ui->chBoxBBatik->setChecked(backends.contains(Backend::Batik));
    ui->chBoxBJSVG->setChecked(backends.contains(Backend::JSVG));
    ui->chBoxBSVGSalamander->setChecked(backends.contains(Backend::SVGSalamander));
    ui->chBoxBEchoSVG->setChecked(backends.contains(Backend::EchoSVG));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Export");

    adjustSize();
}

ExportDialog::~ExportDialog()
{
    delete ui;
}

ExportDialog::Options ExportDialog::options() const
{
    Options opt;
    opt.showTitle = ui->chBoxShowTitle->isChecked();
    opt.indicateStatus = ui->chBoxIndicateStatus->isChecked();
    opt.showDiff = ui->chBoxShowDiff->isChecked();

    if (ui->chBoxBReference->isChecked())   { opt.backends << Backend::Reference; }
    if (ui->chBoxBBatik->isChecked())       { opt.backends << Backend::Batik; }
    if (ui->chBoxBJSVG->isChecked())        { opt.backends << Backend::JSVG; }
    if (ui->chBoxBSVGSalamander->isChecked())        { opt.backends << Backend::SVGSalamander; }
    if (ui->chBoxBEchoSVG->isChecked())        { opt.backends << Backend::EchoSVG; } 

    return opt;
}
