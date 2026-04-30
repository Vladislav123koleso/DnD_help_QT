#include "classcard.h"
#include "ui_classcard.h"
#include <QPixmap>
#include <QFile>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

ClassCard::ClassCard(const QString &className, const QString &imgNormal, const QString &imgHover, const QString &desc, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ClassCard),
    pathNormal(imgNormal),
    pathHover(imgHover),
    isHovered(false)
{
    ui->setupUi(this);

    ui->cardFrame->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->imageContainer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->imageLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->descOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // Initial Setup
    ui->nameLabel->setText(className);
    ui->descOverlay->setText(desc);
    
    // Explicitly hide overlay initially
    ui->descOverlay->hide();

    ui->imageLabel->setScaledContents(false);
    
    // Ensure overlay is aligned to bottom
    ui->gridLayout->addWidget(ui->descOverlay, 0, 0, Qt::AlignBottom);

    QTimer::singleShot(0, this, [this](){
        updateImage();
    });
}

ClassCard::~ClassCard()
{
    delete ui;
}

void ClassCard::updateImage()
{
    QString imagePath = isHovered ? pathHover : pathNormal;
    if (QFile::exists(imagePath)) {
        QPixmap pix(imagePath);
        if (!pix.isNull()) {
            QSize targetSize = ui->imageContainer->size();
            if (targetSize.isEmpty()) targetSize = QSize(150, 180); 

            QPixmap scaledPix = pix.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            
            QPixmap finalPix(targetSize);
            finalPix.fill(Qt::transparent);
            
            QPainter painter(&finalPix);
            painter.setRenderHint(QPainter::Antialiasing);
            
            QPainterPath path;
            path.addRoundedRect(0, 0, targetSize.width(), targetSize.height(), 10, 10); 
            painter.setClipPath(path);
            
            int x = (targetSize.width() - scaledPix.width()) / 2;
            int y = (targetSize.height() - scaledPix.height()) / 2;
            
            painter.drawPixmap(x, y, scaledPix);
            
            ui->imageLabel->setPixmap(finalPix);
            ui->imageLabel->setText(QString());
        }
    } else {
        ui->imageLabel->clear();
        ui->imageLabel->setText(QString());
    }

    if (isHovered) {
        ui->descOverlay->show();
        ui->cardFrame->setStyleSheet("QFrame#cardFrame { border: 2px solid #555; border-radius: 10px; background: transparent; }");
    } else {
        ui->descOverlay->hide();
        ui->cardFrame->setStyleSheet("QFrame#cardFrame { border: 1px solid #ccc; border-radius: 10px; background: transparent; }");
    }
}

void ClassCard::enterEvent(QEnterEvent *event)
{
    isHovered = true;
    updateImage();
    QWidget::enterEvent(event);
}

void ClassCard::leaveEvent(QEvent *event)
{
    isHovered = false;
    updateImage();
    QWidget::leaveEvent(event);
}

void ClassCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit classSelected(ui->nameLabel->text());
    }
    QWidget::mousePressEvent(event);
}

void ClassCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateImage();
}

void ClassCard::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this](){
        updateImage();
    });
}
