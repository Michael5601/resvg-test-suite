#pragma once

#include <QString>

#include "tests.h"

enum class BuildType
{
    Debug,
    Release,
};

class Settings
{
public:
    void load() noexcept;
    void save() const noexcept;

    QString resvgPath() const noexcept;
    QString resultsPath() const noexcept;
    QString testsPath() const noexcept;

public:
    TestSuite testSuite = TestSuite::Own;
    BuildType buildType = BuildType::Debug;
    QString customTestsPath;
    int viewSize = 250;
    bool useBatik = true;
    bool useJSVG = true;
    bool useSVGSalamander = true;
    QString resvgDir; // it's a dir, not a path
    QString batikPath;
    QString jsvgPath;
    QString svgsalamanderPath;
};
