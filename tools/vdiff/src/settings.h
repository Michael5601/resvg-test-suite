#pragma once

#include <QString>

#include "tests.h"

class Settings
{
public:
    void load() noexcept;
    void save() const noexcept;

    QString resultsPath() const noexcept;
    QString testsPath() const noexcept;

public:
    TestSuite testSuite = TestSuite::Own;
    QString customTestsPath;
    int viewSizeWidth = 240;
    int viewSizeHeight = 180;
    bool useBatik = true;
    bool useJSVG = true;
    bool useSVGSalamander = true;
    bool useEchoSVG = true;
    QString batikPath;
    QString jsvgPath;
    QString svgsalamanderPath;
    QString echosvgPath;
};
