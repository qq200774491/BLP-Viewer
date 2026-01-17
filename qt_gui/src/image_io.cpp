#include "image_io.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "blp_api.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {

QString extensionFromPath(const QString& path) {
    return QFileInfo(path).suffix().toLower();
}

void appendBytes(void* context, void* data, int size) {
    auto* out = reinterpret_cast<QByteArray*>(context);
    out->append(reinterpret_cast<const char*>(data), size);
}

QByteArray rgbaToRgbBytes(const RgbaImage& image) {
    QByteArray rgb;
    if (image.width <= 0 || image.height <= 0) {
        return rgb;
    }

    const int pixelCount = image.width * image.height;
    rgb.resize(pixelCount * 3);

    const unsigned char* src = reinterpret_cast<const unsigned char*>(image.pixels.constData());
    unsigned char* dst = reinterpret_cast<unsigned char*>(rgb.data());

    for (int i = 0; i < pixelCount; ++i) {
        dst[i * 3 + 0] = src[i * 4 + 0];
        dst[i * 3 + 1] = src[i * 4 + 1];
        dst[i * 3 + 2] = src[i * 4 + 2];
    }

    return rgb;
}

bool encodeToBytes(const RgbaImage& image,
                   const QString& format,
                   int quality,
                   QByteArray* outBytes,
                   QString* outError) {
    if (!outBytes) {
        if (outError) {
            *outError = "输出缓冲区为空";
        }
        return false;
    }

    outBytes->clear();

    if (image.width <= 0 || image.height <= 0 || image.pixels.isEmpty()) {
        if (outError) {
            *outError = "无效的图像数据";
        }
        return false;
    }

    const QString fmt = normalizeFormat(format);

    if (fmt == "png") {
        const int stride = image.width * 4;
        int ok = stbi_write_png_to_func(appendBytes,
                                        outBytes,
                                        image.width,
                                        image.height,
                                        4,
                                        image.pixels.constData(),
                                        stride);
        if (!ok && outError) {
            *outError = "PNG 编码失败";
        }
        return ok != 0;
    }

    if (fmt == "tga") {
        int ok = stbi_write_tga_to_func(appendBytes,
                                        outBytes,
                                        image.width,
                                        image.height,
                                        4,
                                        image.pixels.constData());
        if (!ok && outError) {
            *outError = "TGA 编码失败";
        }
        return ok != 0;
    }

    if (fmt == "bmp") {
        const QByteArray rgb = rgbaToRgbBytes(image);
        int ok = stbi_write_bmp_to_func(appendBytes,
                                        outBytes,
                                        image.width,
                                        image.height,
                                        3,
                                        rgb.constData());
        if (!ok && outError) {
            *outError = "BMP 编码失败";
        }
        return ok != 0;
    }

    if (fmt == "jpg") {
        const QByteArray rgb = rgbaToRgbBytes(image);
        const int clampedQuality = qBound(1, quality, 100);
        int ok = stbi_write_jpg_to_func(appendBytes,
                                        outBytes,
                                        image.width,
                                        image.height,
                                        3,
                                        rgb.constData(),
                                        clampedQuality);
        if (!ok && outError) {
            *outError = "JPG 编码失败";
        }
        return ok != 0;
    }

    if (outError) {
        *outError = "不支持的输出格式";
    }
    return false;
}

} // namespace

QStringList supportedExtensions() {
    return {"blp", "png", "jpg", "jpeg", "bmp", "tga"};
}

QString normalizeFormat(const QString& format) {
    const QString fmt = format.trimmed().toLower();
    if (fmt == "jpeg") {
        return "jpg";
    }
    return fmt;
}

bool isSupportedFile(const QString& path) {
    const QString ext = normalizeFormat(extensionFromPath(path));
    return supportedExtensions().contains(ext);
}

bool loadImageFile(const QString& path,
                   RgbaImage* outImage,
                   ImageMeta* outMeta,
                   QString* outError,
                   BlpApi* blpApi) {
    if (!outImage) {
        if (outError) {
            *outError = "输出图像为空";
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) {
            *outError = "打开文件失败";
        }
        return false;
    }

    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        if (outError) {
            *outError = "文件为空";
        }
        return false;
    }

    const QString ext = normalizeFormat(extensionFromPath(path));

    if (ext == "blp") {
        if (!blpApi || !blpApi->ensureLoaded(outError)) {
            if (outError && outError->isEmpty()) {
                *outError = "BLP 库未加载";
            }
            return false;
        }

        BlpImage blpImage = {};
        const BlpResult result = blpApi->loadFromBuffer(bytes, &blpImage);
        if (result != BLP_SUCCESS) {
            if (outError) {
                *outError = "BLP 解码失败";
            }
            return false;
        }

        outImage->width = static_cast<int>(blpImage.width);
        outImage->height = static_cast<int>(blpImage.height);
        outImage->pixels = QByteArray(reinterpret_cast<const char*>(blpImage.data),
                                      static_cast<int>(blpImage.data_len));
        blpApi->freeImage(&blpImage);
    } else {
        int width = 0;
        int height = 0;
        int comp = 0;
        stbi_uc* data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes.constData()),
            bytes.size(),
            &width,
            &height,
            &comp,
            4);

        if (!data) {
            if (outError) {
                *outError = "图像解码失败";
            }
            return false;
        }

        outImage->width = width;
        outImage->height = height;
        outImage->pixels = QByteArray(reinterpret_cast<const char*>(data), width * height * 4);
        stbi_image_free(data);
    }

    if (outMeta) {
        outMeta->width = outImage->width;
        outMeta->height = outImage->height;
        outMeta->format = ext;
        outMeta->fileSize = QFileInfo(path).size();
    }

    return true;
}

bool writeImageFile(const QString& outputPath,
                    const QString& format,
                    const RgbaImage& image,
                    int quality,
                    int mipCount,
                    QString* outError,
                    BlpApi* blpApi) {
    const QString fmt = normalizeFormat(format);
    const QFileInfo outInfo(outputPath);
    QDir().mkpath(outInfo.absolutePath());

    if (fmt == "blp") {
        if (!blpApi || !blpApi->ensureLoaded(outError)) {
            if (outError && outError->isEmpty()) {
                *outError = "BLP 库未加载";
            }
            return false;
        }

        QByteArray pngBytes;
        if (!encodeToBytes(image, "png", 100, &pngBytes, outError)) {
            return false;
        }

        const int clampedQuality = qBound(0, quality, 100);
        return blpApi->encodePngBytesToBlp(pngBytes,
                                           outputPath,
                                           clampedQuality,
                                           mipCount,
                                           outError);
    }

    QByteArray encoded;
    if (!encodeToBytes(image, fmt, quality, &encoded, outError)) {
        return false;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outError) {
            *outError = "打开输出文件失败";
        }
        return false;
    }

    if (file.write(encoded) != encoded.size()) {
        if (outError) {
            *outError = "写入输出文件失败";
        }
        return false;
    }

    return true;
}

QImage rgbaToQImage(const RgbaImage& image) {
    if (image.width <= 0 || image.height <= 0 || image.pixels.isEmpty()) {
        return QImage();
    }

    QImage view(reinterpret_cast<const uchar*>(image.pixels.constData()),
                image.width,
                image.height,
                QImage::Format_RGBA8888);
    return view.copy();
}

QString formatFileSize(quint64 bytes) {
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;

    if (bytes >= gb) {
        return QString::number(bytes / gb, 'f', 2) + " GB";
    }
    if (bytes >= mb) {
        return QString::number(bytes / mb, 'f', 2) + " MB";
    }
    if (bytes >= kb) {
        return QString::number(bytes / kb, 'f', 2) + " KB";
    }
    return QString::number(bytes) + " B";
}
