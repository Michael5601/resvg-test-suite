#include <QSettings>
#include <QFileInfo>

#include "settings.h"

namespace Key {
    static const QString TestSuite          = "TestSuite";
    static const QString CustomTestsPath    = "CustomTestsPath";
    static const QString BatikPath          = "BatikPath";
    static const QString JSVGPath           = "JSVGPath";
    static const QString SVGSalamanderPath  = "SVGSalamanderPath";
    static const QString EchoSVGPath        = "EchoSVGPath";
    static const QString UseBatik           = "UseBatik";
    static const QString UseJSVG            = "UseJSVG";
    static const QString UseSVGSalamander   = "UseSVGSalamander";
    static const QString UseEchoSVG         = "UseEchoSVG";
    static const QString ViewSize           = "ViewSize";
}

static QString testSuiteToStr(TestSuite t) noexcept
{
    switch (t) {
        case TestSuite::Own      : return "results";
        case TestSuite::Custom   : return "custom";
    }

    Q_UNREACHABLE();
}

static TestSuite testSuiteFromStr(const QString &str) noexcept
{
    if (str == "custom") {
        return TestSuite::Custom;
    } else {
        return TestSuite::Own;
    }

    Q_UNREACHABLE();
}

void Settings::load() noexcept
{
    QSettings appSettings;
    this->testSuite = testSuiteFromStr(appSettings.value(Key::TestSuite).toString());
    this->customTestsPath = appSettings.value(Key::CustomTestsPath).toString();

    this->useBatik = appSettings.value(Key::UseBatik).toBool();
    this->useJSVG = appSettings.value(Key::UseJSVG).toBool();
    this->useSVGSalamander = appSettings.value(Key::UseSVGSalamander).toBool();
    this->useEchoSVG = appSettings.value(Key::UseEchoSVG).toBool();

    this->batikPath = appSettings.value(Key::BatikPath).toString();
    this->jsvgPath = appSettings.value(Key::JSVGPath).toString();
    this->svgsalamanderPath = appSettings.value(Key::SVGSalamanderPath).toString();
    this->echosvgPath = appSettings.value(Key::EchoSVGPath).toString();
}

void Settings::save() const noexcept
{
    QSettings appSettings;
    appSettings.setValue(Key::TestSuite, testSuiteToStr(this->testSuite));
    appSettings.setValue(Key::CustomTestsPath, this->customTestsPath);
    appSettings.setValue(Key::ViewSize, this->viewSize);
    appSettings.setValue(Key::UseBatik, this->useBatik);
    appSettings.setValue(Key::UseJSVG, this->useJSVG);
    appSettings.setValue(Key::UseSVGSalamander, this->useSVGSalamander);
    appSettings.setValue(Key::UseEchoSVG, this->useEchoSVG);
    appSettings.setValue(Key::BatikPath, this->batikPath);
    appSettings.setValue(Key::JSVGPath, this->jsvgPath);
    appSettings.setValue(Key::EchoSVGPath, this->echosvgPath);
    appSettings.setValue(Key::SVGSalamanderPath, this->svgsalamanderPath);
}

QString Settings::resultsPath() const noexcept
{
    Q_ASSERT(!QString(SRCDIR).isEmpty());
    const auto path = QString("%1/../../%2.csv").arg(SRCDIR).arg(testSuiteToStr(this->testSuite));

    Q_ASSERT(QFile::exists(path));

    return QFileInfo(path).absoluteFilePath();
}

QString Settings::testsPath() const noexcept
{
    QString path;
    switch (this->testSuite) {
        case TestSuite::Own      : path = QString("%1/../../tests").arg(SRCDIR); break;
        case TestSuite::Custom   : path = QString("%1/../../eclipse-tests").arg(SRCDIR); break;
    }

    Q_ASSERT(QFile::exists(path));
    return QFileInfo(path).absoluteFilePath();
}
