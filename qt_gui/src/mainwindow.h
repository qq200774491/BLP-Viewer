#pragma once

#include <QListWidget>
#include <QMainWindow>
#include <QSet>

#include "blp_api.h"

class QLabel;
class QLineEdit;
class QListWidgetItem;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class ImageView;
class FileListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit FileListWidget(QWidget* parent = nullptr);

signals:
    void filesDropped(const QStringList& paths);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onAddFiles();
    void onAddFolder();
    void onRemoveSelected();
    void onClearList();
    void onBrowseInputDir();
    void onBrowseOutputDir();
    void onConvertAll();
    void onSelectionChanged(QListWidgetItem* current, QListWidgetItem* previous);
    void onFormatChanged();
    void onZoomSliderChanged(int value);
    void onFitClicked();
    void onResetZoomClicked();

private:
    void setupUi();
    void applyStyle();
    void addFiles(const QStringList& paths);
    void addFolderFiles(const QString& folder, bool recursive);
    void updatePreview(const QString& path);
    void updateInfoBar(const QString& path);
    void updateBlpStatus();
    void logMessage(const QString& message);
    QString buildOutputPath(const QString& inputPath, const QString& format, bool overwrite) const;
    QString normalizedFormat() const;

    FileListWidget* fileList_ = nullptr;
    ImageView* imageView_ = nullptr;

    QLineEdit* inputDirEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QComboBox* formatCombo_ = nullptr;
    QLabel* qualityLabel_ = nullptr;
    QLabel* qualityValueLabel_ = nullptr;
    QSlider* qualitySlider_ = nullptr;
    QSpinBox* mipSpin_ = nullptr;
    QCheckBox* overwriteCheck_ = nullptr;
    QCheckBox* recursiveCheck_ = nullptr;

    QLabel* infoLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;
    QLabel* blpLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPlainTextEdit* logEdit_ = nullptr;
    QSlider* zoomSlider_ = nullptr;

    QSet<QString> fileSet_;
    BlpApi blpApi_;
};
