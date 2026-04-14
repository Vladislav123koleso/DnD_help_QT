#include "racecard.h"
#include "ui_racecard.h"
#include <QPixmap>
#include <QFile>
#include <QDebug>
#include <QMouseEvent>

 #include <QPainter>
#include <QPainterPath>

#include <QTimer>

RaceCard::RaceCard(const QString &raceName, const QString &imgNormal, const QString &imgHover, const QString &desc, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RaceCard),
    pathNormal(imgNormal),
    pathHover(imgHover),
    isHovered(false)
{
    ui->setupUi(this);

    // Initial Setup
    ui->nameLabel->setText(raceName);
    ui->descOverlay->setText(desc);
    
    // Explicitly hide overlay initially
    ui->descOverlay->hide();

    // IMPORTANT: If we are manually drawing rounded pixmaps, we usually don't want scaledContents to stretch a small image.
    // However, if we regenerate the pixmap on resize, setting it to false is safer so we control the scaling.
    // Or true if we accept slight stretching between resize events.
    ui->imageLabel->setScaledContents(false);
    
    // Ensure overlay is aligned to bottom
    ui->gridLayout->addWidget(ui->descOverlay, 0, 0, Qt::AlignBottom);

    // Initial state update - Use a timer to wait for the layout to settle and calculate correct sizes
    QTimer::singleShot(0, this, [this](){
        updateImage();
    });
}

RaceCard::~RaceCard()
{
    delete ui;
}

void RaceCard::updateImage()
{
    QString imagePath = isHovered ? pathHover : pathNormal;
    if (QFile::exists(imagePath)) {
        QPixmap pix(imagePath);
        if (!pix.isNull()) {
            // Use current size of the imageContainer (or a safeguard minimum if not yet shown)
            QSize targetSize = ui->imageContainer->size();
            if (targetSize.isEmpty()) targetSize = QSize(150, 180); // Fallback

            // Scale source image to COVER the target area (KeepAspectRatioByExpanding)
            QPixmap scaledPix = pix.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            
            // Prepare a blank transparent pixmap of the EXACT target size
            QPixmap finalPix(targetSize);
            finalPix.fill(Qt::transparent);
            
            QPainter painter(&finalPix);
            painter.setRenderHint(QPainter::Antialiasing);
            
            // Create rounding path
            QPainterPath path;
            path.addRoundedRect(0, 0, targetSize.width(), targetSize.height(), 10, 10); 
            painter.setClipPath(path);
            
            // Draw the scaled image centered/cropped
            // Since we used KeepAspectRatioByExpanding, scaledPix might be larger than targetSize in one dimension.
            // We need to draw it centered.
            int x = (targetSize.width() - scaledPix.width()) / 2;
            int y = (targetSize.height() - scaledPix.height()) / 2;
            
            painter.drawPixmap(x, y, scaledPix);
            
            ui->imageLabel->setPixmap(finalPix);
        }
    } else {
        ui->imageLabel->setText("Img?");
    }

    if (isHovered) {
        ui->descOverlay->show();
        ui->cardFrame->setStyleSheet("QFrame#cardFrame { border: 2px solid #555; border-radius: 10px; background: transparent; }");
    } else {
        ui->descOverlay->hide();
        ui->cardFrame->setStyleSheet("QFrame#cardFrame { border: 1px solid #ccc; border-radius: 10px; background: transparent; }");
    }
}

void RaceCard::enterEvent(QEnterEvent *event)
{
    isHovered = true;
    updateImage();
    QWidget::enterEvent(event);
}

void RaceCard::leaveEvent(QEvent *event)
{
    isHovered = false;
    updateImage();
    QWidget::leaveEvent(event);
}

void RaceCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateImage(); // Regenerate image with new size
}

void RaceCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit raceSelected(ui->nameLabel->text());
    }
    QWidget::mousePressEvent(event);
}

void RaceCard::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateImage(); // Ensure image is updated when widget is shown
}

