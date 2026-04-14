#ifndef CLASSCARD_H
#define CLASSCARD_H

#include <QWidget>
#include <QEnterEvent>

namespace Ui {
class ClassCard;
}

class ClassCard : public QWidget
{
    Q_OBJECT

public:
    explicit ClassCard(const QString &className, const QString &imgNormal, const QString &imgHover, const QString &desc, QWidget *parent = nullptr);
    ~ClassCard();

signals:
    void classSelected(const QString &className);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Ui::ClassCard *ui;
    QString pathNormal;
    QString pathHover;
    bool isHovered;
    
    void updateImage();
};

#endif // CLASSCARD_H