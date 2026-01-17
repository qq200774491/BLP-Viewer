#include "imageview.h"

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QPainter>
#include <QWheelEvent>

ImageView::ImageView(QWidget* parent)
    : QGraphicsView(parent),
      scene_(new QGraphicsScene(this)) {
    setScene(scene_);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setBackgroundBrush(checkerboardBrush());
    updatePlaceholder();
}

void ImageView::setImage(const QImage& image) {
    scene_->clear();
    pixmapItem_ = scene_->addPixmap(QPixmap::fromImage(image));
    placeholder_ = nullptr;
    scene_->setSceneRect(pixmapItem_->boundingRect());
    fitMode_ = true;
    fitToImage();
    emit imageChanged(true);
}

void ImageView::clearImage() {
    scene_->clear();
    pixmapItem_ = nullptr;
    zoom_ = 1.0;
    fitMode_ = true;
    updatePlaceholder();
    emit zoomChanged(zoom_);
    emit imageChanged(false);
}

void ImageView::fitToImage() {
    if (!pixmapItem_) {
        return;
    }
    fitInView(pixmapItem_, Qt::KeepAspectRatio);
    zoom_ = transform().m11();
    fitMode_ = true;
    emit zoomChanged(zoom_);
}

void ImageView::setZoom(double value) {
    if (!pixmapItem_) {
        return;
    }
    zoom_ = qBound(0.05, value, 20.0);
    resetTransform();
    scale(zoom_, zoom_);
    fitMode_ = false;
    emit zoomChanged(zoom_);
}

double ImageView::zoom() const {
    return zoom_;
}

void ImageView::wheelEvent(QWheelEvent* event) {
    if (!pixmapItem_) {
        event->ignore();
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    const double factor = delta > 0 ? 1.15 : (1.0 / 1.15);
    setZoom(zoom_ * factor);
    event->accept();
}

void ImageView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (pixmapItem_ && fitMode_) {
        fitToImage();
    } else if (!pixmapItem_) {
        updatePlaceholder();
    }
}

void ImageView::updatePlaceholder() {
    if (!scene_) {
        return;
    }

    scene_->clear();
    scene_->setSceneRect(QRectF(QPointF(0, 0), viewport()->size()));
    placeholder_ = scene_->addText("拖放图片到这里预览");
    placeholder_->setDefaultTextColor(QColor(90, 96, 110));

    const QRectF bounds = sceneRect();
    const QRectF textRect = placeholder_->boundingRect();
    placeholder_->setPos(bounds.center().x() - textRect.width() / 2.0,
                         bounds.center().y() - textRect.height() / 2.0);
}

QBrush ImageView::checkerboardBrush() const {
    const int tileSize = 16;
    QPixmap pixmap(tileSize * 2, tileSize * 2);
    pixmap.fill(QColor(224, 228, 235));

    QPainter painter(&pixmap);
    painter.fillRect(0, 0, tileSize, tileSize, QColor(190, 195, 205));
    painter.fillRect(tileSize, tileSize, tileSize, tileSize, QColor(190, 195, 205));
    painter.end();

    return QBrush(pixmap);
}
