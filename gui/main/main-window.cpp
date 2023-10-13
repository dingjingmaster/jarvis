//
// Created by dingjing on 23-6-29.
//

#include "main-window.h"

#include <QDebug>
#include <QX11Info>
#include <QMouseEvent>
#include <QApplication>
#include <QPropertyAnimation>

#include "system-tray.h"
#include "main-header.h"
#include "xatom-helper.h"

#include <X11/Xlib.h>

MainWindow::MainWindow(QWidget *parent)
    : QWidget (parent), mDirection(NONE)
{
    setMouseTracking (true);
    setContentsMargins (0, 0, 0, 0);
    setMinimumSize (mMinWidth, mMinHeight);
    setWindowFlag (Qt::FramelessWindowHint);
//    setAttribute (Qt::WA_TranslucentBackground);
    setWindowIcon(QIcon(":/img/icons/tray-icon.png"));

#if 0
    setWindowFlags (Qt::Dialog
        | Qt::WindowStaysOnTopHint
        | Qt::WindowTransparentForInput
        | Qt::FramelessWindowHint
        | Qt::WindowDoesNotAcceptFocus);
#endif

    mHeader = new MainHeader;
    mHeaderAnimation = new QPropertyAnimation(mHeader, "headerHeight");

    mMainLayout = new QVBoxLayout;
    mMainLayout->setContentsMargins (0, 0, 0, 0);

    mHeader->hide();

    mMainLayout->addWidget (mHeader);
    mMainLayout->addStretch ();


    setLayout (mMainLayout);

    mTray = new SystemTray(this);
    mTray->show();


    connect(mHeader, &MainHeader::windowClose,   this, [&] () {
        qApp->quit();
    });

    static bool maxWin = false;
    connect(mHeader, &MainHeader::windowMin,     this, [&] () {
        maxWin = false;
        setWindowState(Qt::WindowMinimized);
    });

    connect(mHeader, &MainHeader::windowMax,     this, [&] () {
        if (maxWin) {
            maxWin = false;
            setWindowState(windowState () & ~Qt::WindowMaximized);
            setWindowState(Qt::WindowNoState);
        }
        else {
            maxWin = true;
            setWindowState(Qt::WindowMaximized);
        }
    });
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent (e);
}

void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    showHeader (e);
    QPoint globalPos = e->globalPos();
    QRect rect = this->rect();
    QPoint tl = mapToGlobal (rect.topLeft());
    QPoint rb = mapToGlobal (rect.bottomRight());

    qreal dpiRatio = qApp->devicePixelRatio ();
    if (!mIsPress) {
        region (globalPos);
    }
    else {
        if (NONE != mDirection) {
            QRect rMove (tl, rb);
            switch (mDirection) {
                case LEFT: {
                    if (rb.x () - globalPos.x () <= this->minimumWidth ()) {
                        rMove.setX (tl.x ());
                    }
                    else {
                        rMove.setX (globalPos.x ());
                    }
                    break;
                }
                case RIGHT: {
                    rMove.setWidth (globalPos.x () - tl.x ());
                    break;
                }
                case UP: {
                    if (rb.y () - globalPos.y () <= this->minimumHeight ()) {
                        rMove.setY (tl.y ());
                    }
                    else {
                        rMove.setY (globalPos.y ());
                    }
                    break;
                }
                case DOWN: {
                    rMove.setHeight (globalPos.y () - tl.y ());
                    break;
                }
                case LEFT_TOP: {
                    if (rb.x () - globalPos.x () <= this->minimumWidth ()) {
                        rMove.setX (tl.x ());
                    }
                    else {
                        rMove.setX (globalPos.x ());
                    }

                    if (rb.y () - globalPos.y () <= this->minimumHeight ()) {
                        rMove.setY (tl.y ());
                    }
                    else {
                        rMove.setY (globalPos.y ());
                    }
                    break;
                }
                case RIGHT_TOP: {
                    rMove.setWidth (globalPos.x () - tl.x ());
                    rMove.setY (globalPos.y ());
                    break;
                }
                case LEFT_BOTTOM: {
                    rMove.setX (globalPos.x ());
                    rMove.setHeight (globalPos.y () - tl.y ());
                    break;
                }
                case RIGHT_BOTTOM: {
                    rMove.setWidth (globalPos.x () - tl.x ());
                    rMove.setHeight (globalPos.y () - tl.y ());
                    break;
                }
                default: {
                    break;
                }
            }
            this->setGeometry(rMove);
            return;
        }
    }

    if (mDrag) {
        if (QX11Info::isPlatformX11 ()) {
            Display *display = QX11Info::display ();
            Atom netMoveResize = XInternAtom (display, "_NET_WM_MOVERESIZE", False);
            XEvent xEvent;
            const auto pos = QCursor::pos ();
            memset (&xEvent, 0, sizeof (XEvent));
            xEvent.xclient.type = ClientMessage;
            xEvent.xclient.message_type = netMoveResize;
            xEvent.xclient.display = display;
            xEvent.xclient.window = this->winId ();
            xEvent.xclient.format = 32;
            xEvent.xclient.data.l[0] = pos.x () * dpiRatio;
            xEvent.xclient.data.l[1] = pos.y () * dpiRatio;
            xEvent.xclient.data.l[2] = 8;
            xEvent.xclient.data.l[3] = Button1;
            xEvent.xclient.data.l[4] = 0;

            XUngrabPointer (display, CurrentTime);
            XSendEvent (display, QX11Info::appRootWindow (QX11Info::appScreen ()), False,
                        SubstructureNotifyMask | SubstructureRedirectMask, &xEvent);
            XEvent xevent;
            memset (&xevent, 0, sizeof (XEvent));

            xevent.type = ButtonRelease;
            xevent.xbutton.button = Button1;
            xevent.xbutton.window = this->winId ();
            xevent.xbutton.x = e->pos ().x () * dpiRatio;
            xevent.xbutton.y = e->pos ().y () * dpiRatio;
            xevent.xbutton.x_root = pos.x () * dpiRatio;
            xevent.xbutton.y_root = pos.y () * dpiRatio;
            xevent.xbutton.display = display;

            XSendEvent (display, this->effectiveWinId (), False, ButtonReleaseMask, &xevent);
            XFlush (display);

            if (e->source () == Qt::MouseEventSynthesizedByQt) {
                if (!MainWindow::mouseGrabber ()) {
                    this->grabMouse ();
                    this->releaseMouse ();
                }
            }
            mDrag = false;
        }
        else {
            move ((QCursor::pos () - mOffset) * dpiRatio);
        }
    }
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    QWidget::mousePressEvent (e);

    if (e->isAccepted()) {
        return;
    }

    switch (e->button()) {
        case Qt::LeftButton: {
            mDrag = true;
            mIsPress = true;
            mOffset = mapFromGlobal (QCursor::pos());
            if (NONE != mDirection) {
                mouseGrabber();
            }
            else {
                mDragPos = e->globalPos() - frameGeometry().topLeft();
            }
            break;
        }
        case Qt::RightButton:
        default:{
            QWidget::mousePressEvent (e);
        }
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton
        || e->button() == Qt::RightButton) {
        mDrag = false;
        mIsPress = false;
        if (NONE != mDirection) {
            releaseMouse();
            setCursor (QCursor(Qt::ArrowCursor));
        }
    }
    else {
        QWidget::mouseReleaseEvent (e);
    }
}

void MainWindow::region(const QPoint &cursorGlobalPoint)
{
    QRect rect = this->rect();
    QPoint tl = mapToGlobal(rect.topLeft());
    QPoint rb = mapToGlobal(rect.bottomRight());
    int x = cursorGlobalPoint.x();
    int y = cursorGlobalPoint.y();

//    qDebug() << "region -- tl: " << tl << " rb: " << rb << " x: " << x << " b: " << y;

    if (tl.x() + 3 >= x && x >= tl.x() - 3 && tl.y() + 3 >= y && y >= tl.y() - 3) {
        mDirection = LEFT_TOP;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));  // 设置鼠标形状
    }
    else if (x >= rb.x() - 3 && x <= rb.x() + 3 && y >= rb.y() - 3 && y <= rb.y() + 3) {
        mDirection = RIGHT_BOTTOM;
        this->setCursor(QCursor(Qt::SizeFDiagCursor));
    }
    else if (x <= tl.x() + 3 && x >= tl.x() - 3 && y >= rb.y() - 3 && y <= rb.y() + 3) {
        mDirection = LEFT_BOTTOM;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    }
    else if (x <= rb.x() + 3 && x >= rb.x() - 3 && y >= tl.y() + 3 && y <= tl.y() - 3) {
        mDirection = RIGHT_TOP;
        this->setCursor(QCursor(Qt::SizeBDiagCursor));
    }
    else if (x <= tl.x() + 3 && x >= tl.x() - 3) {
        mDirection = LEFT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    }
    else if (x <= rb.x() + 3 && x >= rb.x() - 3) {
        mDirection = RIGHT;
        this->setCursor(QCursor(Qt::SizeHorCursor));
    }
    else if (y >= tl.y() - 3 && y <= tl.y() + 3) {
        mDirection = UP;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    }
    else if (y <= rb.y() + 3 && y >= rb.y() - 3) {
        mDirection = DOWN;
        this->setCursor(QCursor(Qt::SizeVerCursor));
    }
    else {
        mDirection = NONE;
        this->setCursor(QCursor(Qt::ArrowCursor));
    }
}

void MainWindow::showHeader(QMouseEvent *e)
{
    auto posT = e->pos();

#if 0 //DEBUG
    headerToShow();
#else

    if (mDrag) {
        if (mHeader->height() < mHeader->maximumHeight() + 20) {
            headerToShow();
        }
    }
    else {
        if (posT.x() > 0 && posT.x() < width()
            && posT.y() > 0 && posT.y() < mHeader->maximumHeight() + 20) {
            headerToShow();
        }
//        else if (mHeader->height() > 0) {
//            headerToHidden();
//        }
    }
#endif
}

void MainWindow::leaveEvent(QEvent *e)
{
    headerToHidden();
    QWidget::leaveEvent (e);
}

void MainWindow::headerToShow()
{
    if (0 == mHeader->height() || !mHeader->isVisible()) {
        mHeader->setHeaderHeight (0);
        mHeader->show();
        mHeaderAnimation->setDuration(1000);
        mHeaderAnimation->setStartValue(0);
        mHeaderAnimation->setEndValue(mHeader->maximumHeight());
        mHeaderAnimation->start ();
    }
}

void MainWindow::headerToHidden()
{
    if (mHeader->height() > 0 || mHeader->isVisible()) {
        mHeaderAnimation->setDuration(1000);
        mHeaderAnimation->setStartValue(mHeader->maximumHeight());
        mHeaderAnimation->setEndValue(0);
        mHeaderAnimation->start ();
    }
}
