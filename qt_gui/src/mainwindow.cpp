#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QEvent>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QUrl>
#include <QtEndian>
#include <QVector>

#include "blp_api.h"
#include "image_io.h"
#include "imageview.h"

namespace {

QStringList extractLocalPaths(const QMimeData* mimeData) {
    QStringList paths;
    if (!mimeData || !mimeData->hasUrls()) {
        return paths;
    }

    const QList<QUrl> urls = mimeData->urls();
    for (const QUrl& url : urls) {
        const QString localPath = url.toLocalFile();
        if (!localPath.isEmpty()) {
            paths << localPath;
        }
    }
    return paths;
}

bool isPowerOfTwo(int value) {
    return value > 0 && ((value & (value - 1)) == 0);
}

struct BlpMipEntry {
    int index = 0;
    quint32 offset = 0;
    quint32 size = 0;
};

QVector<BlpMipEntry> readBlpMipEntries(const QByteArray& bytes) {
    QVector<BlpMipEntry> entries;
    if (bytes.size() < 148) {
        return entries;
    }

    const char* data = bytes.constData();
    if (memcmp(data, "BLP1", 4) != 0 && memcmp(data, "BLP2", 4) != 0) {
        return entries;
    }

    const int offsetsOffset = 20;
    const int sizesOffset = offsetsOffset + 16 * 4;

    for (int i = 0; i < 16; ++i) {
        const quint32 offset = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(data + offsetsOffset + i * 4));
        const quint32 size = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(data + sizesOffset + i * 4));
        if (offset != 0 && size != 0) {
            entries.push_back({i, offset, size});
        }
    }

    return entries;
}

const char* kInfoNormalStyle =
    "font-size: 15px;"
    "font-weight: 600;"
    "color: #1e2127;"
    "padding: 8px 10px;"
    "background: #eef2f8;"
    "border-radius: 6px;";

const char* kInfoWarnStyle =
    "font-size: 15px;"
    "font-weight: 600;"
    "color: #c0392b;"
    "padding: 8px 10px;"
    "background: #fff0f0;"
    "border-radius: 6px;";

} // namespace

FileListWidget::FileListWidget(QWidget* parent)
    : QListWidget(parent) {
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAcceptDrops(true);
    setDragEnabled(false);
    setDropIndicatorShown(true);
}

void FileListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FileListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FileListWidget::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QStringList paths = extractLocalPaths(event->mimeData());

    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        event->acceptProposedAction();
        return;
    }

    event->ignore();
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUi();
    applyStyle();
    qApp->installEventFilter(this);
    updateBlpStatus();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QStringList paths = extractLocalPaths(event->mimeData());

    addFiles(paths);
    event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    const auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget || (widget != this && !isAncestorOf(widget))) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
        auto* dragEvent = static_cast<QDragEnterEvent*>(event);
        if (dragEvent->mimeData()->hasUrls()) {
            dragEvent->acceptProposedAction();
            return true;
        }
    }

    if (event->type() == QEvent::Drop) {
        auto* dropEvent = static_cast<QDropEvent*>(event);
        const QStringList paths = extractLocalPaths(dropEvent->mimeData());
        if (!paths.isEmpty()) {
            addFiles(paths);
            dropEvent->acceptProposedAction();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupUi() {
    setWindowTitle("BLP 查看器");
    setMinimumSize(1200, 760);
    setAcceptDrops(true);

    QWidget* central = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, central);

    QWidget* leftPanel = new QWidget(splitter);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(12);
    leftPanel->setMinimumWidth(360);
    leftPanel->setMaximumWidth(520);

    QGroupBox* fileGroup = new QGroupBox("待处理文件", leftPanel);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileGroup);
    QLabel* fileHint = new QLabel("拖拽图片到任意位置，或使用按钮添加。列表中的文件会参与批量转换。", fileGroup);
    fileHint->setWordWrap(true);
    fileHint->setStyleSheet("color: #5b6472;");
    fileList_ = new FileListWidget(fileGroup);
    fileList_->setMinimumHeight(260);
    QHBoxLayout* fileButtons = new QHBoxLayout();
    QPushButton* addFilesButton = new QPushButton("添加文件", fileGroup);
    QPushButton* addFolderButton = new QPushButton("添加文件夹", fileGroup);
    QPushButton* removeButton = new QPushButton("移除", fileGroup);
    QPushButton* clearButton = new QPushButton("清空", fileGroup);
    fileButtons->addWidget(addFilesButton);
    fileButtons->addWidget(addFolderButton);
    fileButtons->addWidget(removeButton);
    fileButtons->addWidget(clearButton);
    fileLayout->addWidget(fileHint);
    fileLayout->addWidget(fileList_, 1);
    fileLayout->addLayout(fileButtons);
    leftLayout->addWidget(fileGroup, 2);

    QGroupBox* pathGroup = new QGroupBox("批量路径", leftPanel);
    QGridLayout* pathLayout = new QGridLayout(pathGroup);
    pathLayout->setColumnStretch(1, 1);
    pathLayout->setColumnMinimumWidth(1, 320);

    QLabel* inputLabel = new QLabel("输入目录（可选）：", pathGroup);
    inputDirEdit_ = new QLineEdit(pathGroup);
    inputDirEdit_->setPlaceholderText("选择目录后可点击“扫描”加入列表");
    inputDirEdit_->setClearButtonEnabled(true);
    inputDirEdit_->setMinimumWidth(320);
    inputDirEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPushButton* inputBrowse = new QPushButton("选择", pathGroup);
    QPushButton* scanButton = new QPushButton("扫描", pathGroup);
    inputBrowse->setFixedWidth(72);
    scanButton->setFixedWidth(72);

    QLabel* outputLabel = new QLabel("输出目录（必填）：", pathGroup);
    outputDirEdit_ = new QLineEdit(pathGroup);
    outputDirEdit_->setPlaceholderText("选择输出目录");
    outputDirEdit_->setClearButtonEnabled(true);
    outputDirEdit_->setMinimumWidth(320);
    outputDirEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPushButton* outputBrowse = new QPushButton("选择", pathGroup);
    outputBrowse->setFixedWidth(72);

    recursiveCheck_ = new QCheckBox("包含子目录", pathGroup);

    pathLayout->addWidget(inputLabel, 0, 0);
    pathLayout->addWidget(inputDirEdit_, 0, 1);
    pathLayout->addWidget(inputBrowse, 0, 2);
    pathLayout->addWidget(scanButton, 0, 3);
    pathLayout->addWidget(outputLabel, 1, 0);
    pathLayout->addWidget(outputDirEdit_, 1, 1);
    pathLayout->addWidget(outputBrowse, 1, 2);
    pathLayout->addWidget(recursiveCheck_, 1, 3);
    leftLayout->addWidget(pathGroup);

    QGroupBox* convertGroup = new QGroupBox("转换设置", leftPanel);
    QGridLayout* convertLayout = new QGridLayout(convertGroup);

    QLabel* formatLabel = new QLabel("输出格式：", convertGroup);
    formatCombo_ = new QComboBox(convertGroup);
    formatCombo_->addItems({"BLP", "PNG", "JPG", "BMP", "TGA"});

    qualityLabel_ = new QLabel("质量：", convertGroup);
    qualitySlider_ = new QSlider(Qt::Horizontal, convertGroup);
    qualitySlider_->setRange(0, 100);
    qualitySlider_->setValue(90);
    qualityValueLabel_ = new QLabel("90", convertGroup);

    QLabel* mipLabel = new QLabel("层级数量：", convertGroup);
    mipSpin_ = new QSpinBox(convertGroup);
    mipSpin_->setRange(1, 16);
    mipSpin_->setValue(1);

    overwriteCheck_ = new QCheckBox("覆盖已存在文件", convertGroup);
    overwriteCheck_->setChecked(false);

    convertLayout->addWidget(formatLabel, 0, 0);
    convertLayout->addWidget(formatCombo_, 0, 1, 1, 2);
    convertLayout->addWidget(qualityLabel_, 1, 0);
    convertLayout->addWidget(qualitySlider_, 1, 1);
    convertLayout->addWidget(qualityValueLabel_, 1, 2);
    convertLayout->addWidget(mipLabel, 2, 0);
    convertLayout->addWidget(mipSpin_, 2, 1, 1, 2);
    convertLayout->addWidget(overwriteCheck_, 3, 0, 1, 3);

    leftLayout->addWidget(convertGroup);

    QPushButton* convertButton = new QPushButton("开始转换", leftPanel);
    convertButton->setMinimumHeight(36);
    leftLayout->addWidget(convertButton);

    progressBar_ = new QProgressBar(leftPanel);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    leftLayout->addWidget(progressBar_);

    QGroupBox* logGroup = new QGroupBox("日志", leftPanel);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logEdit_ = new QPlainTextEdit(logGroup);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(120);
    logLayout->addWidget(logEdit_);
    leftLayout->addWidget(logGroup, 1);

    QWidget* rightPanel = new QWidget(splitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    infoTitleLabel_ = new QLabel("未加载图像", rightPanel);
    infoTitleLabel_->setStyleSheet(kInfoNormalStyle);
    infoTitleLabel_->setWordWrap(true);
    rightLayout->addWidget(infoTitleLabel_);

    imageView_ = new ImageView(rightPanel);
    rightLayout->addWidget(imageView_, 1);

    QGroupBox* mipGroup = new QGroupBox("BLP 层级", rightPanel);
    QVBoxLayout* mipLayout = new QVBoxLayout(mipGroup);
    mipList_ = new QListWidget(mipGroup);
    mipList_->setSelectionMode(QAbstractItemView::SingleSelection);
    mipList_->setMinimumHeight(90);
    mipList_->setMaximumHeight(160);
    mipLayout->addWidget(mipList_);
    rightLayout->addWidget(mipGroup);

    QHBoxLayout* zoomLayout = new QHBoxLayout();
    QPushButton* fitButton = new QPushButton("适应窗口", rightPanel);
    QPushButton* resetZoomButton = new QPushButton("原始大小", rightPanel);
    zoomSlider_ = new QSlider(Qt::Horizontal, rightPanel);
    zoomSlider_->setRange(10, 400);
    zoomSlider_->setValue(100);
    zoomLayout->addWidget(fitButton);
    zoomLayout->addWidget(resetZoomButton);
    zoomLayout->addWidget(zoomSlider_, 1);
    rightLayout->addLayout(zoomLayout);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({420, 780});

    rootLayout->addWidget(splitter, 1);

    setCentralWidget(central);

    infoLabel_ = new QLabel("未加载图像", this);
    zoomLabel_ = new QLabel("缩放：100%", this);
    blpLabel_ = new QLabel("BLP：未加载", this);

    statusBar()->addWidget(infoLabel_, 1);
    statusBar()->addPermanentWidget(zoomLabel_);
    statusBar()->addPermanentWidget(blpLabel_);

    connect(addFilesButton, &QPushButton::clicked, this, &MainWindow::onAddFiles);
    connect(addFolderButton, &QPushButton::clicked, this, &MainWindow::onAddFolder);
    connect(scanButton, &QPushButton::clicked, this, &MainWindow::onScanInputDir);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveSelected);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearList);
    connect(inputBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseInputDir);
    connect(outputBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(convertButton, &QPushButton::clicked, this, &MainWindow::onConvertAll);
    connect(fileList_, &QListWidget::currentItemChanged, this, &MainWindow::onSelectionChanged);
    connect(fileList_, &FileListWidget::filesDropped, this, &MainWindow::addFiles);
    connect(mipList_, &QListWidget::currentItemChanged, this, &MainWindow::onMipSelectionChanged);
    connect(formatCombo_, &QComboBox::currentTextChanged, this, &MainWindow::onFormatChanged);
    connect(qualitySlider_, &QSlider::valueChanged, this, [this](int value) {
        qualityValueLabel_->setText(QString::number(value));
    });
    connect(zoomSlider_, &QSlider::valueChanged, this, &MainWindow::onZoomSliderChanged);
    connect(fitButton, &QPushButton::clicked, this, &MainWindow::onFitClicked);
    connect(resetZoomButton, &QPushButton::clicked, this, &MainWindow::onResetZoomClicked);
    connect(imageView_, &ImageView::zoomChanged, this, [this](double zoom) {
        const int percent = qBound(10, static_cast<int>(zoom * 100.0 + 0.5), 400);
        zoomSlider_->blockSignals(true);
        zoomSlider_->setValue(percent);
        zoomSlider_->blockSignals(false);
        zoomLabel_->setText(QString("缩放：%1%").arg(percent));
    });
    connect(imageView_, &ImageView::imageChanged, this, [this](bool hasImage) {
        if (!hasImage) {
            zoomSlider_->blockSignals(true);
            zoomSlider_->setValue(100);
            zoomSlider_->blockSignals(false);
            zoomLabel_->setText("缩放：100%");
        }
    });

    onFormatChanged();
    clearPreviewState();
}

void MainWindow::applyStyle() {
    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Window, QColor(245, 246, 248));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(236, 238, 242));
    palette.setColor(QPalette::Button, QColor(45, 108, 223));
    palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
    palette.setColor(QPalette::Text, QColor(30, 33, 39));
    palette.setColor(QPalette::WindowText, QColor(30, 33, 39));
    palette.setColor(QPalette::Highlight, QColor(45, 108, 223));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    qApp->setPalette(palette);

    const QString style =
        "QMainWindow { background: #f5f6f8; }"
        "QLineEdit, QComboBox, QSpinBox, QPlainTextEdit {"
        "  border: 1px solid #d1d5dc;"
        "  border-radius: 6px;"
        "  padding: 4px 6px;"
        "  background: #ffffff;"
        "}"
        "QComboBox::drop-down { border: 0px; }"
        "QPushButton {"
        "  background: #2d6cdf;"
        "  color: white;"
        "  border-radius: 6px;"
        "  padding: 6px 10px;"
        "}"
        "QPushButton:hover { background: #245ec2; }"
        "QPushButton:disabled { background: #9fb6e2; }"
        "QGroupBox {"
        "  border: 1px solid #d7dbe3;"
        "  border-radius: 8px;"
        "  margin-top: 8px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 4px;"
        "  color: #3b3f45;"
        "}"
        "QListWidget {"
        "  border: 1px solid #d1d5dc;"
        "  border-radius: 6px;"
        "  background: #ffffff;"
        "}"
        "QStatusBar { background: #eef0f4; }"
        "QProgressBar {"
        "  border: 1px solid #d1d5dc;"
        "  border-radius: 5px;"
        "  text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #2d6cdf;"
        "  border-radius: 4px;"
        "}";

    qApp->setStyleSheet(style);
}

void MainWindow::onAddFiles() {
    const QString filter = "图片 (*.png *.jpg *.jpeg *.bmp *.tga *.blp)";
    const QStringList files = QFileDialog::getOpenFileNames(this, "添加图片", QString(), filter);
    addFiles(files);
}

void MainWindow::onAddFolder() {
    const QString startDir = inputDirEdit_->text().trimmed();
    const QString folder = QFileDialog::getExistingDirectory(this, "选择文件夹", startDir);
    if (folder.isEmpty()) {
        return;
    }

    inputDirEdit_->setText(folder);
    addFolderFiles(folder, recursiveCheck_->isChecked());
}

void MainWindow::onScanInputDir() {
    const QString folder = inputDirEdit_->text().trimmed();
    if (folder.isEmpty()) {
        logMessage("请先设置输入目录");
        return;
    }

    QFileInfo info(folder);
    if (!info.exists() || !info.isDir()) {
        logMessage("输入目录不存在");
        return;
    }

    addFolderFiles(info.absoluteFilePath(), recursiveCheck_->isChecked());
}

void MainWindow::onRemoveSelected() {
    const QList<QListWidgetItem*> items = fileList_->selectedItems();
    for (QListWidgetItem* item : items) {
        const QString path = item->text();
        fileSet_.remove(path);
        delete fileList_->takeItem(fileList_->row(item));
    }
}

void MainWindow::onClearList() {
    fileSet_.clear();
    fileList_->clear();
    imageView_->clearImage();
    clearPreviewState();
}

void MainWindow::onBrowseInputDir() {
    const QString startDir = inputDirEdit_->text().trimmed();
    const QString folder = QFileDialog::getExistingDirectory(this, "选择输入目录", startDir);
    if (!folder.isEmpty()) {
        inputDirEdit_->setText(folder);
    }
}

void MainWindow::onBrowseOutputDir() {
    const QString startDir = outputDirEdit_->text().trimmed();
    const QString folder = QFileDialog::getExistingDirectory(this, "选择输出目录", startDir);
    if (!folder.isEmpty()) {
        outputDirEdit_->setText(folder);
    }
}

void MainWindow::onConvertAll() {
    if (fileList_->count() == 0) {
        const QString inputDir = inputDirEdit_->text().trimmed();
        if (!inputDir.isEmpty()) {
            addFolderFiles(inputDir, recursiveCheck_->isChecked());
        }
    }

    if (fileList_->count() == 0) {
        logMessage("没有可转换的文件");
        return;
    }

    const QString outputDir = outputDirEdit_->text().trimmed();
    if (outputDir.isEmpty()) {
        logMessage("请先设置输出目录");
        return;
    }

    const QString format = normalizedFormat();
    const int quality = qualitySlider_->value();
    const int mipCount = mipSpin_->value();
    const bool overwrite = overwriteCheck_->isChecked();

    progressBar_->setRange(0, fileList_->count());
    progressBar_->setValue(0);

    int successCount = 0;
    for (int i = 0; i < fileList_->count(); ++i) {
        QListWidgetItem* item = fileList_->item(i);
        const QString inputPath = item->text();

        RgbaImage image;
        ImageMeta meta;
        QString error;
        if (!loadImageFile(inputPath, &image, &meta, &error, &blpApi_)) {
            logMessage(QString("读取失败：%1（%2）").arg(inputPath, error));
            progressBar_->setValue(i + 1);
            qApp->processEvents();
            continue;
        }

        const QString outputPath = buildOutputPath(inputPath, format, overwrite);
        if (!writeImageFile(outputPath, format, image, quality, mipCount, &error, &blpApi_)) {
            logMessage(QString("写入失败：%1（%2）").arg(outputPath, error));
            progressBar_->setValue(i + 1);
            qApp->processEvents();
            continue;
        }

        ++successCount;
        progressBar_->setValue(i + 1);
        qApp->processEvents();
    }

    logMessage(QString("已转换 %1 / %2 个文件").arg(successCount).arg(fileList_->count()));
    updateBlpStatus();
}

void MainWindow::onSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous) {
    Q_UNUSED(previous)
    if (!current) {
        imageView_->clearImage();
        clearPreviewState();
        return;
    }
    updatePreview(current->text());
}

void MainWindow::onMipSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous) {
    Q_UNUSED(previous)
    if (!current || !currentIsBlp_ || currentBlpBytes_.isEmpty()) {
        return;
    }

    bool ok = false;
    const int mipIndex = current->data(Qt::UserRole).toInt(&ok);
    if (!ok || mipIndex < 0) {
        return;
    }
    if (mipIndex == currentMipIndex_) {
        return;
    }

    QString error;
    if (!blpApi_.ensureLoaded(&error)) {
        logMessage(QString("BLP 未加载：%1").arg(error));
        updateBlpStatus();
        return;
    }

    QTemporaryFile temp(QDir::tempPath() + "/blp_mip_XXXXXX.png");
    temp.setAutoRemove(true);
    if (!temp.open()) {
        logMessage("创建临时文件失败");
        return;
    }
    const QString tempPath = temp.fileName();
    temp.close();

    if (!blpApi_.decodeMipToPngFromBuffer(currentBlpBytes_, mipIndex, tempPath, &error)) {
        logMessage(QString("层级解码失败：%1").arg(error));
        return;
    }

    RgbaImage image;
    if (!loadImageFile(tempPath, &image, nullptr, &error, &blpApi_)) {
        logMessage(QString("层级预览失败：%1").arg(error));
        return;
    }

    imageView_->setImage(rgbaToQImage(image));

    ImageMeta displayMeta = currentMeta_;
    displayMeta.width = image.width;
    displayMeta.height = image.height;
    setInfoText(displayMeta, mipIndex);
    currentMipIndex_ = mipIndex;
}

void MainWindow::onFormatChanged() {
    const QString fmt = normalizedFormat();
    const bool isBlp = (fmt == "blp");
    const bool isJpg = (fmt == "jpg");

    qualitySlider_->setEnabled(isBlp || isJpg);
    qualityLabel_->setText(isBlp ? "BLP 质量：" : (isJpg ? "JPG 质量：" : "质量："));
    qualityValueLabel_->setEnabled(isBlp || isJpg);
    mipSpin_->setEnabled(isBlp);
}

void MainWindow::onZoomSliderChanged(int value) {
    imageView_->setZoom(value / 100.0);
}

void MainWindow::onFitClicked() {
    imageView_->fitToImage();
}

void MainWindow::onResetZoomClicked() {
    imageView_->setZoom(1.0);
}

void MainWindow::addFiles(const QStringList& paths) {
    QStringList pending;

    for (const QString& path : paths) {
        QFileInfo info(path);
        if (!info.exists()) {
            continue;
        }
        if (info.isDir()) {
            addFolderFiles(info.absoluteFilePath(), recursiveCheck_->isChecked());
            continue;
        }
        if (!isSupportedFile(info.absoluteFilePath())) {
            continue;
        }
        const QString fullPath = info.absoluteFilePath();
        if (fileSet_.contains(fullPath)) {
            continue;
        }
        pending << fullPath;
        fileSet_.insert(fullPath);
    }

    for (const QString& path : pending) {
        fileList_->addItem(path);
    }

    if (fileList_->count() > 0 && !fileList_->currentItem()) {
        fileList_->setCurrentRow(0);
    }
}

void MainWindow::addFolderFiles(const QString& folder, bool recursive) {
    if (folder.trimmed().isEmpty()) {
        return;
    }

    QStringList filters;
    for (const QString& ext : supportedExtensions()) {
        filters << QString("*.%1").arg(ext);
    }

    QDirIterator it(folder,
                    filters,
                    QDir::Files,
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

    QStringList paths;
    while (it.hasNext()) {
        paths << it.next();
    }

    addFiles(paths);
}

void MainWindow::updatePreview(const QString& path) {
    clearPreviewState();

    const QFileInfo info(path);
    const QString ext = normalizeFormat(info.suffix());

    if (ext == "blp") {
        currentIsBlp_ = true;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            imageView_->clearImage();
            infoTitleLabel_->setText("打开 BLP 失败");
            infoLabel_->setText("打开 BLP 失败");
            logMessage(QString("打开失败：%1").arg(path));
            return;
        }

        const QByteArray bytes = file.readAll();
        if (bytes.isEmpty()) {
            imageView_->clearImage();
            infoTitleLabel_->setText("BLP 文件为空");
            infoLabel_->setText("BLP 文件为空");
            logMessage(QString("文件为空：%1").arg(path));
            return;
        }

        currentBlpBytes_ = bytes;

        QString error;
        if (!blpApi_.ensureLoaded(&error)) {
            imageView_->clearImage();
            infoTitleLabel_->setText("BLP 库未加载");
            infoLabel_->setText("BLP 库未加载");
            logMessage(QString("BLP 未加载：%1").arg(error));
            updateBlpStatus();
            return;
        }

        BlpImage blpImage = {};
        const BlpResult result = blpApi_.loadFromBuffer(bytes, &blpImage);
        if (result != BLP_SUCCESS) {
            imageView_->clearImage();
            infoTitleLabel_->setText("BLP 解码失败");
            infoLabel_->setText("BLP 解码失败");
            logMessage(QString("BLP 解码失败：%1").arg(path));
            updateBlpStatus();
            return;
        }

        RgbaImage image;
        image.width = static_cast<int>(blpImage.width);
        image.height = static_cast<int>(blpImage.height);
        image.pixels = QByteArray(reinterpret_cast<const char*>(blpImage.data),
                                  static_cast<int>(blpImage.data_len));
        blpApi_.freeImage(&blpImage);

        imageView_->setImage(rgbaToQImage(image));

        currentMeta_.width = image.width;
        currentMeta_.height = image.height;
        currentMeta_.format = "blp";
        currentMeta_.fileSize = info.size();
        currentMipIndex_ = 0;
        setInfoText(currentMeta_, 0);

        const int detected = detectBlpMipCount(bytes);
        const int mipCount = qMax(1, detected);
        mipList_->blockSignals(true);
        mipList_->clear();
        for (int i = 0; i < mipCount; ++i) {
            const int mipWidth = qMax(1, currentMeta_.width >> i);
            const int mipHeight = qMax(1, currentMeta_.height >> i);
            auto* item = new QListWidgetItem(QString("第%1层（%2 x %3）")
                                                 .arg(i)
                                                 .arg(mipWidth)
                                                 .arg(mipHeight));
            item->setData(Qt::UserRole, i);
            mipList_->addItem(item);
        }
        mipList_->setEnabled(true);
        mipList_->setCurrentRow(0);
        mipList_->blockSignals(false);

        updateBlpStatus();
        return;
    }

    RgbaImage image;
    ImageMeta meta;
    QString error;
    if (!loadImageFile(path, &image, &meta, &error, &blpApi_)) {
        imageView_->clearImage();
        infoTitleLabel_->setText("加载图像失败");
        infoLabel_->setText("加载图像失败");
        logMessage(QString("预览失败：%1（%2）").arg(path, error));
        updateBlpStatus();
        return;
    }

    imageView_->setImage(rgbaToQImage(image));
    currentMeta_ = meta;
    setInfoText(meta, -1);

    mipList_->blockSignals(true);
    mipList_->clear();
    mipList_->addItem("仅 BLP 文件显示层级列表");
    mipList_->setEnabled(false);
    mipList_->blockSignals(false);

    updateBlpStatus();
}

void MainWindow::updateInfoBar(const QString& path) {
    QFileInfo info(path);
    if (!info.exists()) {
        infoLabel_->setText("未加载图像");
        return;
    }

    infoLabel_->setText(QString("%1 (%2)").arg(info.fileName(), formatFileSize(info.size())));
}

void MainWindow::updateBlpStatus() {
    if (blpApi_.isLoaded()) {
        const QString version = blpApi_.version();
        blpLabel_->setText(version.isEmpty() ? "BLP：已加载" : QString("BLP：%1").arg(version));
    } else {
        blpLabel_->setText("BLP：未加载");
    }
}

void MainWindow::setInfoText(const ImageMeta& meta, int mipIndex) {
    QString info = QString("%1 x %2 像素 | %3 | %4")
                       .arg(meta.width)
                       .arg(meta.height)
                       .arg(formatFileSize(meta.fileSize))
                       .arg(meta.format.toUpper());

    if (mipIndex >= 0) {
        info += QString(" | 层级 %1").arg(mipIndex);
    }

    const bool isPot = isPowerOfTwo(meta.width) && isPowerOfTwo(meta.height);
    if (!isPot) {
        info += " | 非 2 次幂";
    }

    infoLabel_->setText(info);
    infoTitleLabel_->setText(info);
    infoTitleLabel_->setStyleSheet(isPot ? kInfoNormalStyle : kInfoWarnStyle);
    infoLabel_->setStyleSheet(isPot ? "" : "color: #c0392b;");
}

void MainWindow::clearPreviewState() {
    currentBlpBytes_.clear();
    currentMeta_ = ImageMeta();
    currentIsBlp_ = false;
    currentMipIndex_ = 0;

    if (mipList_) {
        mipList_->blockSignals(true);
        mipList_->clear();
        mipList_->addItem("仅 BLP 文件显示层级");
        mipList_->setEnabled(false);
        mipList_->blockSignals(false);
    }

    if (infoLabel_) {
        infoLabel_->setText("未加载图像");
        infoLabel_->setStyleSheet("");
    }
    if (infoTitleLabel_) {
        infoTitleLabel_->setText("未加载图像");
        infoTitleLabel_->setStyleSheet(kInfoNormalStyle);
    }
}

void MainWindow::logMessage(const QString& message) {
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    logEdit_->appendPlainText(QString("[%1] %2").arg(timestamp, message));
}

QString MainWindow::buildOutputPath(const QString& inputPath, const QString& format, bool overwrite) const {
    const QString outputDir = outputDirEdit_->text().trimmed();
    const QFileInfo inputInfo(inputPath);
    const QString baseName = inputInfo.completeBaseName();
    const QString ext = normalizeFormat(format);

    QString candidate = QDir(outputDir).filePath(QString("%1.%2").arg(baseName, ext));
    if (overwrite || !QFileInfo::exists(candidate)) {
        return candidate;
    }

    int index = 1;
    while (QFileInfo::exists(candidate)) {
        candidate = QDir(outputDir).filePath(QString("%1_%2.%3").arg(baseName).arg(index).arg(ext));
        ++index;
    }

    return candidate;
}

QString MainWindow::normalizedFormat() const {
    return normalizeFormat(formatCombo_->currentText());
}
