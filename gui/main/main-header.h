//
// Created by dingjing on 23-6-29.
//

#ifndef JARVIS_MAIN_HEADER_H
#define JARVIS_MAIN_HEADER_H
#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>
#include <QGraphicsBlurEffect>

#include "header-button.h"

class MainHeader : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int headerHeight
                READ getHeaderHeight
                WRITE setHeaderHeight
                USER true)
public:
    explicit MainHeader (QWidget* parent=nullptr);

    int getHeaderHeight ();
    void setHeaderHeight (int h);

Q_SIGNALS:
    void windowMin();
    void windowMax();
    void windowClose();

protected:
    void mouseDoubleClickEvent(QMouseEvent*) override;

private:
    const int               mMinHeight = 0;
    const int               mMaxHeight = 48;

    QLayout*                mLeftLayout;
    QLayout*                mRightLayout;
    QHBoxLayout*            mMainLayout;

    QLabel*                 mHeaderName;

    HeaderButton*           mMinBtn;
    HeaderButton*           mMaxBtn;
    HeaderButton*           mCloseBtn;

    QGraphicsBlurEffect     mOpacity;
};


#endif //JARVIS_MAIN_HEADER_H
