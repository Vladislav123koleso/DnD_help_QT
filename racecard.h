#ifndef RACECARD_H
#define RACECARD_H

#include <QWidget>
#include <QEnterEvent>

namespace Ui {
class RaceCard;
}

class QPushButton;

class RaceCard : public QWidget
{
    Q_OBJECT

public:
    explicit RaceCard(const QString &raceName, const QString &imgNormal, const QString &imgHover, const QString &desc, QWidget *parent = nullptr);
    ~RaceCard();

signals:
    void raceSelected(const QString &raceName);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Ui::RaceCard *ui;
    QPushButton *selectButton;
    QString pathNormal;
    QString pathHover;
    bool isHovered;
    void updateImage();
    
    void updateImage(bool hover);
};

#endif // RACECARD_H
