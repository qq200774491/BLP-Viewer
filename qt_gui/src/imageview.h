#pragma once

#include <QGraphicsView>
#include <QImage>

class QGraphicsPixmapItem;
class QGraphicsTextItem;

class ImageView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ImageView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void clearImage();
    void fitToImage();
    void setZoom(double value);
    double zoom() const;

signals:
    void zoomChanged(double value);
    void imageChanged(bool hasImage);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updatePlaceholder();
    QBrush checkerboardBrush() const;

    QGraphicsScene* scene_ = nullptr;
    QGraphicsPixmapItem* pixmapItem_ = nullptr;
    QGraphicsTextItem* placeholder_ = nullptr;
    double zoom_ = 1.0;
    bool fitMode_ = true;
};
