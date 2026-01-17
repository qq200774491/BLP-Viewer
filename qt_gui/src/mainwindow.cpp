#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
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
#include <QStatusBar>
#include <QVBoxLayout>
#include <QUrl>

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
    setMinimumSize(1100, 720);
    setAcceptDrops(true);

    QWidget* central = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    QGroupBox* pathGroup = new QGroupBox("批量路径", central);
    QGridLayout* pathLayout = new QGridLayout(pathGroup);
    pathLayout->setColumnStretch(1, 1);

    QLabel* inputLabel = new QLabel("输入目录：", pathGroup);
    inputDirEdit_ = new QLineEdit(pathGroup);
    QPushButton* inputBrowse = new QPushButton("浏览...", pathGroup);

    QLabel* outputLabel = new QLabel("输出目录：", pathGroup);
    outputDirEdit_ = new QLineEdit(pathGroup);
    QPushButton* outputBrowse = new QPushButton("浏览...", pathGroup);

    recursiveCheck_ = new QCheckBox("包含子目录", pathGroup);

    pathLayout->addWidget(inputLabel, 0, 0);
    pathLayout->addWidget(inputDirEdit_, 0, 1);
    pathLayout->addWidget(inputBrowse, 0, 2);
    pathLayout->addWidget(outputLabel, 1, 0);
    pathLayout->addWidget(outputDirEdit_, 1, 1);
    pathLayout->addWidget(outputBrowse, 1, 2);
    pathLayout->addWidget(recursiveCheck_, 2, 1, 1, 2);

    rootLayout->addWidget(pathGroup);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, central);

    QWidget* leftPanel = new QWidget(splitter);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    fileList_ = new FileListWidget(leftPanel);
    fileList_->setMinimumWidth(320);
    leftLayout->addWidget(fileList_, 1);

    QHBoxLayout* fileButtons = new QHBoxLayout();
    QPushButton* addFilesButton = new QPushButton("添加文件", leftPanel);
    QPushButton* addFolderButton = new QPushButton("添加文件夹", leftPanel);
    QPushButton* removeButton = new QPushButton("移除", leftPanel);
    QPushButton* clearButton = new QPushButton("清空", leftPanel);
    fileButtons->addWidget(addFilesButton);
    fileButtons->addWidget(addFolderButton);
    fileButtons->addWidget(removeButton);
    fileButtons->addWidget(clearButton);
    leftLayout->addLayout(fileButtons);

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

    QLabel* mipLabel = new QLabel("Mip 级数：", convertGroup);
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

    logEdit_ = new QPlainTextEdit(leftPanel);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(120);
    leftLayout->addWidget(logEdit_);

    QWidget* rightPanel = new QWidget(splitter);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    imageView_ = new ImageView(rightPanel);
    rightLayout->addWidget(imageView_, 1);

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
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

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
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveSelected);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearList);
    connect(inputBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseInputDir);
    connect(outputBrowse, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(convertButton, &QPushButton::clicked, this, &MainWindow::onConvertAll);
    connect(fileList_, &QListWidget::currentItemChanged, this, &MainWindow::onSelectionChanged);
    connect(fileList_, &FileListWidget::filesDropped, this, &MainWindow::addFiles);
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
    infoLabel_->setText("未加载图像");
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
        infoLabel_->setText("未加载图像");
        return;
    }
    updatePreview(current->text());
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
    RgbaImage image;
    ImageMeta meta;
    QString error;
    if (!loadImageFile(path, &image, &meta, &error, &blpApi_)) {
        imageView_->clearImage();
        infoLabel_->setText("加载图像失败");
        logMessage(QString("预览失败：%1（%2）").arg(path, error));
        updateBlpStatus();
        return;
    }

    const QImage qimage = rgbaToQImage(image);
    imageView_->setImage(qimage);

    const QString info = QString("%1 x %2 像素 | %3 | %4")
                             .arg(meta.width)
                             .arg(meta.height)
                             .arg(formatFileSize(meta.fileSize))
                             .arg(meta.format.toUpper());
    infoLabel_->setText(info);
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
