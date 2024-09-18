#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QImageReader>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrentMap>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>

#include "paths.h"
#include "process.h"
#include "imagecache.h"

#include "render.h"

static QSize guessSvgSize(const QString &path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return QSize();
    }

    QXmlStreamReader reader(&file);
    if (!reader.readNextStartElement()) {
        return QSize();
    }

    if (reader.name() != QLatin1String("svg")) {
        return QSize();
    }

    const auto attributes = reader.attributes();
    const auto viewBoxStr = attributes.value("viewBox");
    const auto widthStr = attributes.value("width");
    const auto heightStr = attributes.value("height");

    const auto vbValues = viewBoxStr.split(' ');
    if (vbValues.size() != 4) {
        return QSize();
    }

    auto width = widthStr.isEmpty() ? vbValues[2].toDouble() : widthStr.toDouble();
    auto height = heightStr.isEmpty() ? vbValues[3].toDouble() : heightStr.toDouble();

    return QSize(width, height);
}

Render::Render(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<RenderResult>("RenderResult");
    qRegisterMetaType<DiffOutput>("DiffOutput");

    connect(&m_watcher1, &QFutureWatcher<RenderResult>::resultReadyAt,
            this, &Render::onImageRendered);
    connect(&m_watcher1, &QFutureWatcher<RenderResult>::finished,
            this, &Render::onImagesRendered);

    connect(&m_watcher2, &QFutureWatcher<DiffOutput>::resultReadyAt,
            this, &Render::onDiffResult);
    connect(&m_watcher2, &QFutureWatcher<DiffOutput>::finished,
            this, &Render::onDiffFinished);
}

void Render::setScale(qreal s)
{
    m_dpiScale = s;
    m_viewSize = m_settings->viewSize * s;
}

void Render::render(const QString &path)
{
    m_imgPath = path;
    m_imgs.clear();
    renderImages();
}

QImage Render::renderReference(const RenderData &data)
{
    const QFileInfo fi(data.imgPath);
    const QString path = fi.absolutePath() + "/" + fi.completeBaseName() + ".png";

    Q_ASSERT(QFile(path).exists());

    const QSize targetSize(data.viewSize, data.viewSize);

    QImage img(path);
    if (img.size() != targetSize) {
        img = img.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return img.convertToFormat(QImage::Format_ARGB32);
}

QImage Render::renderViaChrome(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/chrome.png";

    const QString out = Process::run("node", {
        QString("D:/dev/oomph/Bachelor/Library-Comparison/resvg-test-suite/tools/vdiff") + "../chrome-svgrender/svgrender.js",
        data.imgPath,
        outImg,
        QString::number(data.viewSize)
    }, true);

    if (!out.isEmpty()) {
        qDebug().noquote() << "chrome:" << out;
    }

    return loadImage(outImg);
}

QImage Render::renderViaFirefox(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/firefox.png";

    QString out = Process::run(data.convPath, {
        QString("--window-size=%1,%2").arg(data.viewSize).arg(data.viewSize),
        QString("--screenshot=%1").arg(QFileInfo(outImg).absoluteFilePath()),
        // The SVG file path must be formed as file:/// URL.
        QUrl::fromLocalFile(data.imgPath).toString(),
    }, true);

    if (!out.isEmpty()) {
        auto lines = out.split("\n");
        lines.removeAll("");
        auto warnings = lines.filter("Gtk-Message");
        warnings << lines.filter("plugin-container");
        for (const auto &w : warnings) {
            lines.removeOne(w);
        }

        if (!lines.isEmpty()) {
            out = lines.join("\n");

            if (out.simplified() != "*** You are running in headless mode.") {
                qDebug().noquote() << "firefox:" << out;
            }
        }
    }

    auto image = loadImage(outImg);

    // Crop image. Firefox always produces a rectangular image.
    if (!data.imageSize.isEmpty() && data.imageSize != image.size()) {
        const auto y = (image.height() - data.imageSize.height()) / 2;
        image = image.copy(0, y, data.imageSize.width(), data.imageSize.height());
    }

    return image;
}

QImage Render::renderViaSafari(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/" + QFileInfo(data.imgPath).fileName() + ".png";

    const QString out = Process::run("qlmanage", {
        "-t",
        "-s", QString::number(data.viewSize),
        "-o", Paths::workDir(),
        data.imgPath,
    }, true);

    auto image = loadImage(outImg);

    // Crop image. `qlmanage` always produces a rectangular image.
    if (!data.imageSize.isEmpty() && data.imageSize != image.size()) {
        const auto y = (image.height() - data.imageSize.height()) / 2;
        image = image.copy(0, y, data.imageSize.width(), data.imageSize.height());
    }

    return image;
}

QImage Render::renderViaResvg(const RenderData &data)
{
    const QString outPath = Paths::workDir() + "/resvg.png";

    QString out;
    if (data.testSuite == TestSuite::Own) {
        out = Process::run(data.convPath, {
            data.imgPath,
            outPath,
            "-w", QString::number(data.viewSize),
            "--skip-system-fonts",
            "--use-fonts-dir", QString("D:/dev/oomph/Bachelor/Library-Comparison/resvg-test-suite/tools/vdiff") + "../../fonts",
            "--font-family", "Noto Sans",
            "--serif-family", "Noto Serif",
            "--sans-serif-family", "Noto Sans",
            "--cursive-family", "Yellowtail",
            "--fantasy-family", "Sedgwick Ave Display",
            "--monospace-family", "Noto Mono",
        }, true);
    } else {
        out = Process::run(data.convPath, {
            data.imgPath,
            outPath,
            "-w", QString::number(data.viewSize)
        }, true);
    }

    if (!out.isEmpty()) {
        qDebug().noquote() << "resvg:" << out;
    }

    return loadImage(outPath).convertToFormat(QImage::Format_ARGB32);
}

QImage Render::renderViaSvgNet(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/" + QFileInfo(data.imgPath).completeBaseName() + ".png";

    const QString out = Process::run(data.convPath, {
        data.imgPath,
        outImg
    }, true);

    if (!out.isEmpty()) {
        qDebug().noquote() << "svgnet:" << out;
    }

    return loadImage(outImg);
}

QImage Render::renderViaBatik(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/batik.png";
    
    // Construct the Batik command
    QStringList arguments;
    arguments << "-Djava.awt.headless=true"
              << "-jar"
              << data.convPath
              << "-w" << QString::number(data.viewSize)
	      << "-h" << QString::number(data.viewSize)
              << data.imgPath
              << "-d" << outImg;
    
    const QString out = Process::run("java", arguments, true);
    
    if (!out.contains("success")) {
        qDebug().noquote() << "batik:" << out;
    }

    auto image = loadImage(outImg);

    // Crop image. Batik always produces a rectangular image.
    if (!data.imageSize.isEmpty() && data.imageSize != image.size()) {
        const auto y = (image.height() - data.imageSize.height()) / 2;
        image = image.copy(0, y, data.imageSize.width(), data.imageSize.height());
    }

    return image;
}

QImage Render::renderViaJSVG(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/jsvg.png";
    
    // Construct the JSVG command
    QStringList arguments;
    arguments << "-Djava.awt.headless=true"
              << "-jar"
              << data.convPath
              << "-w" << QString::number(data.viewSize)
	      << "-h" << QString::number(data.viewSize)
              << data.imgPath
              << "-d" << outImg;
    
    const QString out = Process::run("java", arguments, true);
    
    if (!out.contains("success")) {
        qDebug().noquote() << "jsvg:" << out;
    }

    auto image = loadImage(outImg);

    // Crop image. JSVG always produces a rectangular image.
    if (!data.imageSize.isEmpty() && data.imageSize != image.size()) {
        const auto y = (image.height() - data.imageSize.height()) / 2;
        image = image.copy(0, y, data.imageSize.width(), data.imageSize.height());
    }

    return image;
}

QImage Render::renderViaInkscape(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/inkscape.png";

    /*const QString out = */Process::run(data.convPath, {
        data.imgPath,
        "-w", QString::number(data.viewSize),
        "--export-filename=" + outImg
    });
    return loadImage(outImg);
}

QImage Render::renderViaRsvg(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/rsvg.png";

    const QString out = Process::run(data.convPath, {
        "-f", "png",
        "-w", QString::number(data.viewSize),
        data.imgPath,
        "-o", outImg
    });

    if (!out.isEmpty()) {
        qDebug().noquote() << "rsvg:" << out;
    }

    return loadImage(outImg).convertToFormat(QImage::Format_ARGB32);
}

QImage Render::renderViaQtSvg(const RenderData &data)
{
#ifdef Q_OS_WIN
    const auto exePath = QString("D:/dev/oomph/Bachelor/Library-Comparison/resvg-test-suite/tools/vdiff") + "../qtsvgrender/release/qtsvgrender";
#else
    const auto exePath = QString("D:/dev/oomph/Bachelor/Library-Comparison/resvg-test-suite/tools/vdiff") + "../qtsvgrender/qtsvgrender";
#endif
    const auto outImg = Paths::workDir() + "/qtsvg.png";

    const QString out = Process::run(exePath, {
        data.imgPath,
        outImg,
        QString::number(data.viewSize)
    }, true);

    if (!out.isEmpty()) {
        qDebug().noquote() << "qtsvg:" << out;
    }

    return loadImage(outImg);
}

void Render::renderImages()
{
    const auto ts = m_settings->testSuite;

    QVector<RenderData> list;

    // Parsing SVG using QtSvg directly is a bad idea, because it can crash.
    auto imageSize = guessSvgSize(m_imgPath);
    if (imageSize.isEmpty()) {
        imageSize = QSize(m_viewSize, m_viewSize);
    }
    imageSize = imageSize * (float(m_viewSize) / imageSize.width());

    if (ts != TestSuite::Custom) {
        list.append({ Backend::Reference, m_viewSize, imageSize, m_imgPath, QString(), ts });
    }

    list.append({ Backend::Resvg, m_viewSize, imageSize, m_imgPath, m_settings->resvgPath(), ts });

    auto renderCached = [&](const Backend backend, const QString &renderPath) {
        if (ts != TestSuite::Custom) {
            const auto cachedImage = m_imgCache.getImage(backend, m_imgPath);
            if (!cachedImage.isNull()) {
                m_imgs.insert(backend, cachedImage);
                emit imageReady(backend, cachedImage);
            } else {
                list.append({ backend, m_viewSize, imageSize, m_imgPath, renderPath, ts });
            }
        } else {
            list.append({ backend, m_viewSize, imageSize, m_imgPath, renderPath, ts });
        }
    };

    if (m_settings->useChrome) {
        renderCached(Backend::Chrome, QString());
    }

    if (m_settings->useFirefox) {
        renderCached(Backend::Firefox, m_settings->firefoxPath);
    }

    if (m_settings->useSafari) {
        renderCached(Backend::Safari, QString());
    }

    if (m_settings->useBatik) {
        renderCached(Backend::Batik, m_settings->batikPath);
    }

    if (m_settings->useInkscape) {
        renderCached(Backend::Inkscape, m_settings->inkscapePath);
    }

    if (m_settings->useLibrsvg) {
        list.append({ Backend::Librsvg, m_viewSize, imageSize, m_imgPath, m_settings->librsvgPath, ts });
    }

    if (m_settings->useSvgNet) {
        renderCached(Backend::SvgNet, QString());
    }

    if (m_settings->useQtSvg) {
        list.append({ Backend::QtSvg, m_viewSize, imageSize, m_imgPath, QString(), ts });
    }

    if (m_settings->useJSVG) {
        renderCached(Backend::JSVG, m_settings->jsvgPath);
    }

    const auto future = QtConcurrent::mapped(list, &Render::renderImage);
    m_watcher1.setFuture(future);
}

QImage Render::loadImage(const QString &path)
{
    const QImage img(path);
    if (img.isNull()) {
        throw QString("Invalid image: %1").arg(path);
    }

    QFile::remove(path);
    return img;
}

RenderResult Render::renderImage(const RenderData &data)
{
    try {
        QImage img;
        switch (data.type) {
            case Backend::Reference   : img = renderReference(data); break;
            case Backend::Chrome      : img = renderViaChrome(data); break;
            case Backend::Firefox     : img = renderViaFirefox(data); break;
            case Backend::Safari      : img = renderViaSafari(data); break;
            case Backend::Resvg       : img = renderViaResvg(data); break;
            case Backend::SvgNet      : img = renderViaSvgNet(data); break;
            case Backend::Batik       : img = renderViaBatik(data); break;
            case Backend::Inkscape    : img = renderViaInkscape(data); break;
            case Backend::Librsvg     : img = renderViaRsvg(data); break;
            case Backend::QtSvg       : img = renderViaQtSvg(data); break;
            case Backend::JSVG        : img = renderViaJSVG(data); break;
        }

        return { data.type, img };
    } catch (const QString &s) {
        QImage img(data.viewSize, data.viewSize, QImage::Format_ARGB32);
        img.fill(Qt::white);

        QPainter p(&img);
        auto f = p.font();
        f.setPointSize(12);
        p.setFont(f);
        p.drawText(QRect(0, 0, data.viewSize, data.viewSize),
                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   s);
        p.end();

        return { data.type, img };
    } catch (...) {
        Q_UNREACHABLE();
    }
}

static QImage toRGBFormat(const QImage &img, const QColor &bg)
{
    QImage newImg(img.size(), QImage::Format_RGB32);
    newImg.fill(bg);

    QPainter p(&newImg);
    p.drawImage(0, 0, img);
    p.end();
    return newImg;
}

static int colorDistance(const QColor &color1, const QColor &color2)
{
    const int rd = std::pow(color1.red() - color2.red(), 2);
    const int gd = std::pow(color1.green() - color2.green(), 2);
    const int bd = std::pow(color1.blue() - color2.blue(), 2);
    return std::sqrt(rd + gd + bd);
}

DiffOutput Render::diffImage(const DiffData &data)
{
    if (data.img1.size() != data.img2.size()) {
        QString msg = QString("Images size mismatch: %1x%2 != %3x%4 Chrome vs %5")
            .arg(data.img1.width()).arg(data.img1.height())
            .arg(data.img2.width()).arg(data.img2.height())
            .arg(backendToString(data.type));

        qWarning() << msg;
    }

    Q_ASSERT(data.img1.format() == data.img2.format());

    const int w = qMin(data.img1.width(), data.img2.width());
    const int h = qMin(data.img1.height(), data.img2.height());

    const auto img1 = toRGBFormat(data.img1, Qt::white);
    const auto img2 = toRGBFormat(data.img2, Qt::white);

    QImage diffImg(data.img1.size(), QImage::Format_RGB32);
    diffImg.fill(Qt::red);

    for (int y = 0; y < h; ++y) {
        auto s1 = (QRgb*)(img1.constScanLine(y));
        auto s2 = (QRgb*)(img2.constScanLine(y));
        auto s3 = (QRgb*)(diffImg.scanLine(y));

        for (int x = 0; x < w; ++x) {
            QRgb c1 = *s1;
            QRgb c2 = *s2;

            if (colorDistance(c1, c2) > 5) {
                *s3 = qRgb(255, 0, 0);
            } else {
                *s3 = qRgb(255, 255, 255);
            }

            s1++;
            s2++;
            s3++;
        }
    }

    return { data.type, diffImg };
}

void Render::onImageRendered(const int idx)
{
    const auto res = m_watcher1.resultAt(idx);
    m_imgs.insert(res.type, res.img);
    emit imageReady(res.type, res.img);

    if (m_settings->testSuite != TestSuite::Custom) {
        switch (res.type) {
            case Backend::Chrome :
            case Backend::Firefox :
            case Backend::Safari :
            case Backend::Batik :
            case Backend::JSVG :
            case Backend::Inkscape : m_imgCache.setImage(res.type, m_imgPath, res.img); break;
            default : break;
        }
    }
}

void Render::onImagesRendered()
{
    if (m_settings->testSuite == TestSuite::Custom) {
        // Use Chrome as a reference.

        const QImage refImg = m_imgs.value(Backend::Chrome);

        QVector<DiffData> list;
        const auto append = [&](const Backend type){
            if (m_imgs.contains(type) && type != Backend::Chrome) {
                list.append({ type, refImg, m_imgs.value(type) });
            }
        };

        for (int t = (int)Backend::Firefox; t <= (int)Backend::QtSvg; ++t) {
            append((Backend)t);
        }

        const auto future = QtConcurrent::mapped(list, &Render::diffImage);
        m_watcher2.setFuture(future);
    } else {
        const QImage refImg = m_imgs.value(Backend::Reference);

        QVector<DiffData> list;
        const auto append = [&](const Backend type){
            if (m_imgs.contains(type) && type != Backend::Reference) {
                list.append({ type, refImg, m_imgs.value(type) });
            }
        };

        for (int t = (int)Backend::Chrome; t <= (int)Backend::JSVG; ++t) {
            append((Backend)t);
        }

        const auto future = QtConcurrent::mapped(list, &Render::diffImage);
        m_watcher2.setFuture(future);
    }
}

void Render::onDiffResult(const int idx)
{
    const auto v = m_watcher2.resultAt(idx);
    emit diffReady(v.type, v.img);
}

void Render::onDiffFinished()
{
    emit finished();
}
