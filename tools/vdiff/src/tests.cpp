#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QXmlStreamReader>
#include <QDebug>

#include "settings.h"

#include "tests.h"

static TestState stateFormStr(const QString &str)
{
    bool ok = false;
    const int idx = str.toInt(&ok);

    if (!ok) {
        throw QString("Invalid state ID: '%1'").arg(str);
    }

    switch (idx) {
        case 0 : return TestState::Unknown;
        case 1 : return TestState::Passed;
        case 2 : return TestState::Failed;
        case 3 : return TestState::Crashed;
        default: {
            throw QString("Invalid state ID: %1").arg(idx);
        }
    }
}

static QString parseTitle(const QString &path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return QString();
    }

    QString title;
    QXmlStreamReader reader(&file);
    while (!reader.atEnd() && !reader.hasError()) {
        if (reader.readNextStartElement()) {
            if (reader.name() == QLatin1String("title")) {
                reader.readNext();
                title = reader.text().toString();
                break;
            }
        }
    }

    return title;
}

static QString resolveBaseName(const QFileInfo &info)
{
    auto baseName = info.fileName();

    auto dir = info.absoluteDir();
    baseName.prepend(dir.dirName() + "/");
    dir.cdUp();
    baseName.prepend(dir.dirName() + "/");

    return baseName;
}

static QString resolveBaseNameCustom(const QFileInfo &info)
{
    auto baseName = info.fileName();
    
    auto dir = info.absoluteDir();
    baseName.prepend(dir.dirName() + "/");
    dir.cdUp();
    baseName.prepend(dir.dirName() + "/");

    return baseName;
}

Tests Tests::load(const TestSuite testSuite, const QString &path, const QString &testsPath)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        throw QString("Failed to open %1.").arg(path);
    }

    const QString text = file.readAll();

    // A minimal csv parser.
    //
    // We don't care about escape characters, because they are not used.

    Tests tests;

    int row = 1;
    for (const auto &line : text.split('\n')) {
        // Skip title.
        if (row == 1) {
            row++;
            continue;
        }

        if (line.isEmpty()) {
            break;
        }

        const auto items = line.split(',');

        if (items.size() != BackendsCount) {
            throw QString("Invalid columns count at row %1.").arg(row);
        }

        const auto testPath = testsPath + '/' + items.at(0);

        TestItem item;
        item.path = QFileInfo(testPath).absoluteFilePath();
        item.baseName = resolveBaseName(QFileInfo(testPath));

        item.state.insert(Backend::Batik,       stateFormStr(items.at(1)));
        item.state.insert(Backend::JSVG,        stateFormStr(items.at(2)));
        item.state.insert(Backend::SVGSalamander,  stateFormStr(items.at(3)));
        item.state.insert(Backend::EchoSVG,  stateFormStr(items.at(4)));

        if (testSuite == TestSuite::Own) {
            item.title = parseTitle(testPath);
        }

        tests.m_data << item;

        row++;
    }

    return tests;
}

Tests Tests::loadCustom(const QString &path)
{
    // Tests tests;

    // // Load the custom CSV file.
    // QFile file(path);
    // if (!file.open(QFile::ReadOnly)) {
    //     throw QString("Failed to open %1.").arg(path);
    // }

    // const QString text = file.readAll();
    // int row = 1;

    // for (const auto &line : text.split('\n')) {
    //     // Skip title.
    //     if (row == 1) {
    //         row++;
    //         continue;
    //     }

    //     if (line.isEmpty()) {
    //         break;
    //     }

    //     const auto items = line.split(',');

    //     // Adjust column count check if BackendsCount applies here.
    //     if (items.size() != BackendsCount) {
    //        throw QString("Invalid columns count at row %1.").arg(row);
    //     }

    //     TestItem item;
    //     const QString filePath = testsPath + '/' + items.at(0); // Assuming file path is the first column.
    //     item.path = filePath;
    //     item.baseName = QDir(path).relativeFilePath(filePath);

    //     // Fill the state from the CSV.
    //     item.state.insert(Backend::Batik,       stateFormStr(items.at(1)));
    //     item.state.insert(Backend::JSVG,        stateFormStr(items.at(2)));
    //     item.state.insert(Backend::SVGSalamander, stateFormStr(items.at(3)));
    //     item.state.insert(Backend::EchoSVG,     stateFormStr(items.at(4)));

    //     //tests.m_data << item;
    //     row++;
    // }

    // return tests;

    Tests tests;

    static const QStringList filesFilter = { "*.svg", "*.svgz" };

    QStringList paths;
    QDirIterator it(path, filesFilter, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        paths << it.next();
    }

    paths.sort();

    for (const QString &filePath : paths) {
        TestItem item;
        item.path     = filePath;
        item.baseName = QDir(path).relativeFilePath(filePath);
        tests.m_data << item;
    }

    return tests;
}

void Tests::save(const QString &path)
{
    QString text = "title,batik,jsvg,svgsalamander,echosvg\n";
    for (const TestItem &item : m_data) {
        text += item.baseName + ',';
        text += QString::number((int)item.state.value(Backend::Batik))      + ',';
        text += QString::number((int)item.state.value(Backend::JSVG))       + ',';
        text += QString::number((int)item.state.value(Backend::SVGSalamander))  + ',';
        text += QString::number((int)item.state.value(Backend::EchoSVG)) + '\n';
    }

    QFile file(path);
    if (!file.open(QFile::WriteOnly)) {
        throw QString("Failed to open %1.").arg(path);
    }

    file.write(text.toUtf8());
}

static void collectFilesRecursive(const QString &dir, QVector<QFileInfo> &files)
{
    const auto infos = QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : infos) {
        if (fi.isDir()) {
            collectFilesRecursive(fi.absoluteFilePath(), files);
        } else if (fi.suffix() == "svg") {
            files << fi;
        }
    }
}

void Tests::resync(const Settings &settings)
{
    QVector<QFileInfo> files;
    collectFilesRecursive(settings.testsPath(), files);

    auto oldTests = load(settings.testSuite, settings.resultsPath(), settings.testsPath());
    Tests newTests;

    for (const QFileInfo &fi : files) {
        const auto baseName = resolveBaseName(fi);

        bool isExists = false;
        for (const TestItem &test : oldTests) {
            if (test.baseName == baseName) {
                isExists = true;
                newTests.m_data << test;
                break;
            }
        }

        if (!isExists) {
            TestItem item;
            item.path = fi.absoluteFilePath();
            item.baseName = baseName;
            newTests.m_data << item;
        }
    }
    newTests.save(settings.resultsPath());
}

static QString testSuiteToString(const TestSuite &t)
{
    switch (t) {
        case TestSuite::Own :       return "Own";
        case TestSuite::Custom :    return "Custom";
    }

    Q_UNREACHABLE();
}

QDebug operator<<(QDebug dbg, const TestSuite &t)
{
    return dbg << QString("TestSuite(%1)").arg(testSuiteToString(t));
}

QString backendToString(const Backend &t)
{
    switch (t) {
        case Backend::Reference :   return "Reference";
        case Backend::Batik :       return "Batik";
        case Backend::JSVG :        return "JSVG";
        case Backend::EchoSVG : return "EchoSVG";
        case Backend::SVGSalamander : return "SVGSalamander";
    }

    Q_UNREACHABLE();
}

QDebug operator<<(QDebug dbg, const Backend &t)
{
    return dbg << QString("Backend(%1)").arg(backendToString(t));
}
