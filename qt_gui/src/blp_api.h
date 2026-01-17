#pragma once

#include <QByteArray>
#include <QLibrary>
#include <QString>

#include "blp_lib.h"

class BlpApi {
public:
    BlpApi();

    bool ensureLoaded(QString* outError);
    bool isLoaded() const;
    QString libraryPath() const;
    QString version() const;

    BlpResult loadFromBuffer(const QByteArray& data, BlpImage* outImage);
    void freeImage(BlpImage* image);

    bool encodePngBytesToBlp(const QByteArray& pngBytes,
                             const QString& outputPath,
                             int quality,
                             int mipCount,
                             QString* outError);

    bool decodeMipToPngFromBuffer(const QByteArray& blpBytes,
                                  int mipIndex,
                                  const QString& outputPath,
                                  QString* outError);

private:
    using LoadFromBufferFn = BlpResult (*)(const uint8_t*, uint32_t, BlpImage*);
    using FreeImageFn = void (*)(BlpImage*);
    using GetVersionFn = const char* (*)();
    using EncodeBytesToBlpFn = BlpResult (*)(const uint8_t*, uint32_t, const char*, uint8_t, uint32_t);
    using DecodeMipToPngFromBufferFn = BlpResult (*)(const uint8_t*, uint32_t, uint32_t, const char*);

    bool resolveSymbols(QString* outError);
    bool tryLoadLibrary(const QString& path, QString* outError);
    QStringList candidateLibraryPaths() const;

    QLibrary lib_;
    bool loaded_ = false;
    QString loadedPath_;

    LoadFromBufferFn loadFromBuffer_ = nullptr;
    FreeImageFn freeImage_ = nullptr;
    GetVersionFn getVersion_ = nullptr;
    EncodeBytesToBlpFn encodeBytesToBlp_ = nullptr;
    DecodeMipToPngFromBufferFn decodeMipToPngFromBuffer_ = nullptr;
};
