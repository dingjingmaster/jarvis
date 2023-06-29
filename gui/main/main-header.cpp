//
// Created by dingjing on 23-6-29.
//

#include "main-header.h"

#include <QDebug>


MainHeader::MainHeader(QWidget *parent)
    : QWidget (parent)
{
    setMouseTracking(true);
    setObjectName("main-header");
    setMaximumHeight (mMaxHeight);
    setMinimumHeight (mMinHeight);

    setContentsMargins(0, 0, 0, 0);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WidgetAttribute::WA_StyledBackground);

    mOpacity.setParent (this);
    mOpacity.setBlurHints (QGraphicsBlurEffect::AnimationHint);
    mOpacity.setBlurRadius(9000);

    setStyleSheet("background-color:rgb(204, 48, 31);");
    setAutoFillBackground (true);

    mMainLayout = new QHBoxLayout;
    mMainLayout->setContentsMargins(8, 0, 3, 0);

    mHeaderName = new QLabel;
    auto font = mHeaderName->font();
    font.setBold (true);
    mHeaderName->setFont (font);
    mHeaderName->setStyleSheet ("color:white");
    mHeaderName->setObjectName("main-header-title");

    mLeftLayout = new QHBoxLayout;
    mRightLayout = new QHBoxLayout;
    mRightLayout->setSpacing(3);

    // title
    mHeaderName->setText(tr(WINDOW_TITLE));
    mLeftLayout->addWidget(mHeaderName);

    // button
    mMinBtn = new HeaderButton(nullptr, HeaderButton::MIN);
    mMaxBtn = new HeaderButton(nullptr, HeaderButton::MAX);
    mCloseBtn = new HeaderButton(nullptr, HeaderButton::CLOSE);

    mRightLayout->addWidget(mMinBtn);
    mRightLayout->addWidget(mMaxBtn);
    mRightLayout->addWidget(mCloseBtn);

    // main layout
    mMainLayout->addItem(mLeftLayout);
    mMainLayout->addStretch();
    mMainLayout->addItem(mRightLayout);
    setLayout(mMainLayout);

    connect(mMinBtn,    &QPushButton::clicked, this, &MainHeader::windowMin);
    connect(mMaxBtn,    &QPushButton::clicked, this, &MainHeader::windowMax);
    connect(mCloseBtn,  &QPushButton::clicked, this, &MainHeader::windowClose);
}

int MainHeader::getHeaderHeight()
{
    return height();
}

void MainHeader::setHeaderHeight(int h)
{
    resize (width(), h);
}

void MainHeader::mouseDoubleClickEvent(QMouseEvent *)
{
    Q_EMIT windowMax();
}


