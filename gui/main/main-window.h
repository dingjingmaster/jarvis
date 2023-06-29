//
// Created by dingjing on 23-6-29.
//

#ifndef JARVIS_MAIN_WINDOW_H
#define JARVIS_MAIN_WINDOW_H
#include <QWidget>
#include <QVBoxLayout>


class SystemTray;
class MainHeader;
class QPropertyAnimation;
class MainWindow : public QWidget
{
    Q_OBJECT
    enum Direction {UP = 0, DOWN, LEFT, RIGHT, LEFT_TOP, LEFT_BOTTOM, RIGHT_BOTTOM, RIGHT_TOP, NONE};
public:
    explicit MainWindow (QWidget* parent=nullptr);

protected:
    void leaveEvent (QEvent* e) override;
    void resizeEvent (QResizeEvent* e) override;
    void mouseMoveEvent (QMouseEvent* e) override;
    void mousePressEvent (QMouseEvent* e) override;
    void mouseReleaseEvent (QMouseEvent* e) override;

private:
    void showHeader (QMouseEvent* e);
    void headerToShow ();
    void headerToHidden ();

private:
    void region (const QPoint& cursorGlobalPoint);

private:
    const int                       mMinWidth = 300;
    const int                       mMinHeight = 100;

    SystemTray*                     mTray = nullptr;
    MainHeader*                     mHeader = nullptr;
    QVBoxLayout*                    mMainLayout = nullptr;
    QPropertyAnimation*             mHeaderAnimation = nullptr;

    QPoint                          mOffset;
    QPoint                          mDragPos;
    Direction                       mDirection;
    bool                            mDrag = false;
    bool                            mIsPress = false;
};


#endif //JARVIS_MAIN_WINDOW_H
