#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QStringList>

class BlpApi;

struct RgbaImage {
    int width = 0;
    int height = 0;
    QByteArray pixels;
};

struct ImageMeta {
    int width = 0;
    int height = 0;
    QString format;
    quint64 fileSize = 0;
};

QStringList supportedExtensions();
QString normalizeFormat(const QString& format);
bool isSupportedFile(const QString& path);

bool loadImageFile(const QString& path,
                   RgbaImage* outImage,
                   ImageMeta* outMeta,
                   QString* outError,
                   BlpApi* blpApi);

bool writeImageFile(const QString& outputPath,
                    const QString& format,
                    const RgbaImage& image,
                    int quality,
                    int mipCount,
                    QString* outError,
                    BlpApi* blpApi);

QImage rgbaToQImage(const RgbaImage& image);
QString formatFileSize(quint64 bytes);
