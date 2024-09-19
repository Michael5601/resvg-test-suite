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

QImage Render::renderViaBatik(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/batik.png";
    
    // Construct the Batik command
    QStringList arguments;
    arguments << "-Djava.awt.headless=true"
              << "-jar"
              << data.convPath
              << QString::number(data.viewSize)
	          << QString::number(data.viewSize)
              << data.imgPath
              << outImg;
    
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
              << QString::number(data.viewSize)
	          << QString::number(data.viewSize)
              << data.imgPath
              << outImg;
    
    const QString out = Process::run("java", arguments, true);

    auto image = loadImage(outImg);

    // Crop image. JSVG always produces a rectangular image.
    if (!data.imageSize.isEmpty() && data.imageSize != image.size()) {
        const auto y = (image.height() - data.imageSize.height()) / 2;
        image = image.copy(0, y, data.imageSize.width(), data.imageSize.height());
    }

    return image;
}

QImage Render::renderViaSVGSalamander(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/svgsalamander.png";
    
    // Construct the SVGSalamander command
    QStringList arguments;
    arguments << "-Djava.awt.headless=true"
              << "-jar"
              << data.convPath
              << QString::number(data.viewSize)
	          << QString::number(data.viewSize)
              << data.imgPath
              << outImg;
    
    const QString out = Process::run("java", arguments, true);

    auto image = loadImage(outImg);

    // Crop image. SVGSalamander always produces a rectangular image.
    if (!data.imageSize.isEmpty() && data.imageSize != image.size()) {
        const auto y = (image.height() - data.imageSize.height()) / 2;
        image = image.copy(0, y, data.imageSize.width(), data.imageSize.height());
    }

    return image;
}

QImage Render::renderViaEchoSVG(const RenderData &data)
{
    const auto outImg = Paths::workDir() + "/echosvg.png";
    
    // Construct the EchoSVG command
    QStringList arguments;
    arguments << "-Djava.awt.headless=true"
              << "-jar"
              << data.convPath
              << QString::number(data.viewSize)
	          << QString::number(data.viewSize)
              << data.imgPath
              << outImg;

    const QString out = Process::run("java", arguments, true);
    
    if (!out.contains("success")) {
        qDebug().noquote() << "echosvg:" << out;
    }

    auto image = loadImage(outImg);

    // Crop image. EchoSVG always produces a rectangular image.
    if (!data.imageSize.isEmpty() && data.imageSize != image.size()) {
        const auto y = (image.height() - data.imageSize.height()) / 2;
        image = image.copy(0, y, data.imageSize.width(), data.imageSize.height());
    }

    return image;
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

    if (m_settings->useBatik) {
        renderCached(Backend::Batik, m_settings->batikPath);
    }

    if (m_settings->useJSVG) {
        renderCached(Backend::JSVG, m_settings->jsvgPath);
    }

    if (m_settings->useSVGSalamander) {
        renderCached(Backend::SVGSalamander, m_settings->svgsalamanderPath);
    }

    if (m_settings->useEchoSVG) {
        renderCached(Backend::EchoSVG, m_settings->echosvgPath);
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
            case Backend::Batik       : img = renderViaBatik(data); break;
            case Backend::JSVG        : img = renderViaJSVG(data); break;
            case Backend::SVGSalamander  : img = renderViaSVGSalamander(data); break;
            case Backend::EchoSVG  : img = renderViaEchoSVG(data); break;
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
            case Backend::Batik :
            case Backend::JSVG :
            case Backend::SVGSalamander :
            case Backend::EchoSVG : m_imgCache.setImage(res.type, m_imgPath, res.img); break;
            default : break;
        }
    }
}

void Render::onImagesRendered()
{
    if (m_settings->testSuite == TestSuite::Custom) {
        // Use Chrome as a reference.

       // const QImage refImg = m_imgs.value(Backend::Chrome);

       // QVector<DiffData> list;
       // const auto append = [&](const Backend type){
       //     if (m_imgs.contains(type) && type != Backend::Chrome) {
       //         list.append({ type, refImg, m_imgs.value(type) });
       //     }
       // };

        //for (int t = (int)Backend::Firefox; t <= (int)Backend::QtSvg; ++t) {
        //    append((Backend)t);
        //}

       // const auto future = QtConcurrent::mapped(list, &Render::diffImage);
       // m_watcher2.setFuture(future);
    } else {
        const QImage refImg = m_imgs.value(Backend::Reference);

        QVector<DiffData> list;
        const auto append = [&](const Backend type){
            if (m_imgs.contains(type) && type != Backend::Reference) {
                list.append({ type, refImg, m_imgs.value(type) });
            }
        };

        for (int t = (int)Backend::Batik; t <= (int)Backend::EchoSVG; ++t) {
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
