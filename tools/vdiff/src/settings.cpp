#include <QSettings>
#include <QFileInfo>

#include "settings.h"

namespace Key {
    static const QString TestSuite          = "TestSuite";
    static const QString CustomTestsPath    = "CustomTestsPath";
    static const QString ResvgBuild         = "ResvgBuild";
    static const QString ResvgDir           = "ResvgDir";
    static const QString BatikPath          = "BatikPath";
    static const QString JSVGPath           = "JSVGPath";
    static const QString SVGSalamanderPath  = "SVGSalamanderPath";
    static const QString UseBatik           = "UseBatik";
    static const QString UseJSVG            = "UseJSVG";
    static const QString UseSVGSalamander   = "UseSVGSalamander";
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

static QString buildTypeToStr(BuildType t) noexcept
{
    switch (t) {
        case BuildType::Debug   : return "debug";
        case BuildType::Release : return "release";
    }

    Q_UNREACHABLE();
}

void Settings::load() noexcept
{
    QSettings appSettings;
    this->testSuite = testSuiteFromStr(appSettings.value(Key::TestSuite).toString());
    this->customTestsPath = appSettings.value(Key::CustomTestsPath).toString();

    this->buildType = appSettings.value(Key::ResvgBuild).toString() == "release"
                        ? BuildType::Release
                        : BuildType::Debug;

    this->useBatik = appSettings.value(Key::UseBatik).toBool();
    this->useJSVG = appSettings.value(Key::UseJSVG).toBool();
    this->useSVGSalamander = appSettings.value(Key::UseSVGSalamander).toBool();

    this->resvgDir = appSettings.value(Key::ResvgDir).toString();
    this->batikPath = appSettings.value(Key::BatikPath).toString();
    this->svgsalamanderPath = appSettings.value(Key::SVGSalamanderPath).toString();
}

void Settings::save() const noexcept
{
    QSettings appSettings;
    appSettings.setValue(Key::TestSuite, testSuiteToStr(this->testSuite));
    appSettings.setValue(Key::CustomTestsPath, this->customTestsPath);
    appSettings.setValue(Key::ResvgBuild, buildTypeToStr(this->buildType));
    appSettings.setValue(Key::ViewSize, this->viewSize);
    appSettings.setValue(Key::UseBatik, this->useBatik);
    appSettings.setValue(Key::UseJSVG, this->useJSVG);
    appSettings.setValue(Key::UseSVGSalamander, this->useSVGSalamander);
    appSettings.setValue(Key::ResvgDir, this->resvgDir);
    appSettings.setValue(Key::BatikPath, this->batikPath);
    appSettings.setValue(Key::JSVGPath, this->jsvgPath);
    appSettings.setValue(Key::SVGSalamanderPath, this->svgsalamanderPath);
}

QString Settings::resvgPath() const noexcept
{
    return QString("%1/target/%2/resvg").arg(this->resvgDir, buildTypeToStr(this->buildType));
}

QString Settings::resultsPath() const noexcept
{
    Q_ASSERT(!QString(SRCDIR).isEmpty());
    Q_ASSERT(this->testSuite != TestSuite::Custom);

    const auto path = QString("%1/../../%2.csv").arg(SRCDIR).arg(testSuiteToStr(this->testSuite));

    Q_ASSERT(QFile::exists(path));

    return QFileInfo(path).absoluteFilePath();
}

QString Settings::testsPath() const noexcept
{
    QString path;
    switch (this->testSuite) {
        case TestSuite::Own      : path = QString("%1/../../tests").arg(SRCDIR); break;
        case TestSuite::Custom   : Q_UNREACHABLE();
    }

    Q_ASSERT(QFile::exists(path));
    return QFileInfo(path).absoluteFilePath();
}
