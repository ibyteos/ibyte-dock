#include "appitem.h"
#include "utils/utils.h"
#include "utils/xcbmisc.h"
#include "utils/appwindowmanager.h"
#include "utils/docksettings.h"

#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QToolTip>
#include <KWindowSystem>

AppItem::AppItem(DockEntry *entry, QWidget *parent)
    : DockItem(parent),
      m_entry(entry),
      m_popupTimer(new QTimer(this)),
      m_updateIconGeometryTimer(new QTimer(this)),
      m_openAction(new QAction(tr("Open"))),
      m_closeAction(new QAction(tr("Close All"))),
      m_dockAction(new QAction("")),
      m_popupWidget(new BlurWindow)
{
    m_updateIconGeometryTimer->setInterval(2000);
    m_updateIconGeometryTimer->setSingleShot(true);

    m_popupTimer->setInterval(200);
    m_popupTimer->setSingleShot(true);

    m_contextMenu.addAction(m_openAction);
    m_contextMenu.addAction(m_dockAction);
    m_contextMenu.addAction(m_closeAction);

    initDockAction();
    refreshIcon();
    initStates();

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &AppItem::initStates);
    connect(m_updateIconGeometryTimer, &QTimer::timeout, this, &AppItem::updateWindowIconGeometries, Qt::QueuedConnection);
    connect(m_popupTimer, &QTimer::timeout, this, &AppItem::showPopup);
    connect(m_closeAction, &QAction::triggered, this, &AppItem::closeWindow);
    connect(m_dockAction, &QAction::triggered, this, &AppItem::dockActionTriggered);
    connect(m_openAction, &QAction::triggered, this, [=] {
        AppWindowManager::instance()->openApp(m_entry->className);
    });
}

void AppItem::closeWindow()
{
    for (quint64 id : m_entry->WIdList) {
        AppWindowManager::instance()->closeWindow(id);
    }
}

void AppItem::update()
{
    updateWindowIconGeometries();
    initDockAction();
    refreshIcon();

    QWidget::update();
}

void AppItem::initDockAction()
{
    m_openAction->setVisible(m_entry->WIdList.isEmpty());
    m_closeAction->setVisible(!m_entry->WIdList.isEmpty());

    if (m_entry->isDocked) {
        m_dockAction->setText(tr("UnDock"));
    } else {
        m_dockAction->setText(tr("Dock"));
    }
}

void AppItem::dockActionTriggered()
{
    if (m_entry->isDocked) {
        AppWindowManager::instance()->undock(m_entry);
    }

    m_entry->isDocked = !m_entry->isDocked;

    initDockAction();
    AppWindowManager::instance()->save();
}

void AppItem::refreshIcon()
{
    const int iconSize = qMin(width(), height());
    const QString iconName = m_entry->iconName;

    m_iconPixmap = Utils::getIcon(iconName, iconSize * 0.7, devicePixelRatioF());

    QWidget::update();

    m_updateIconGeometryTimer->start();
}

void AppItem::updateWindowIconGeometries()
{
    const QRect r(mapToGlobal(QPoint(0, 0)),
                  mapToGlobal(QPoint(width(), height())));
     auto *xcb_misc = XcbMisc::instance();

    for (quint64 id : m_entry->WIdList) {
         xcb_misc->set_window_icon_geometry(id, r);
    }
}

void AppItem::initStates()
{
    m_entry->isActive = m_entry->WIdList.contains(KWindowSystem::activeWindow());
    update();
}

void AppItem::showPopup()
{
    if (m_entry->className.isEmpty())
        return;

    QPoint p = mapToGlobal(QPoint(0, 0));

    m_popupWidget->setText(m_entry->className);
    m_popupWidget->setVisible(true);

    if (DockSettings::instance()->position() == DockSettings::Bottom)
        p += QPoint(width() / 2 - m_popupWidget->width() / 2, 0);
    else
        p += QPoint(0, height() / 2 - m_popupWidget->height() / 2);

    m_popupWidget->move(p);
    m_popupWidget->update();
}

void AppItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setPen(Qt::NoPen);

    // draw background
    const QRectF itemRect = rect();
    const int lineWidth = itemRect.width() * 0.3;
    const int lineHeight = 2;

    if (!m_entry->WIdList.isEmpty()) {
        QRect activeRect;
        if (DockSettings::instance()->position() == DockSettings::Left) {
            activeRect = QRect(0,
                               (itemRect.height() - lineWidth) / 2,
                               lineHeight, lineWidth);
        } else {
            activeRect = QRect((itemRect.width() - lineWidth) / 2,
                               itemRect.height() - lineHeight - 1,
                               lineWidth, lineHeight);
        }

        if (m_entry->isActive) {
            painter.setBrush(QColor("#1974FF"));
            painter.drawRect(activeRect);
        } else {
            painter.setBrush(QColor("#0F0F0F"));
            painter.drawRect(activeRect);
        }
    }

    const auto ratio = devicePixelRatioF();
    const qreal iconX = rect().center().x() - m_iconPixmap.rect().center().x() / ratio;
    const qreal iconY = rect().center().y() - m_iconPixmap.rect().center().y() / ratio;
    painter.drawPixmap(iconX, iconY, m_iconPixmap);
}

void AppItem::mousePressEvent(QMouseEvent *e)
{
    m_popupWidget->hide();
    m_updateIconGeometryTimer->stop();

    if (e->button() == Qt::RightButton) {
        m_contextMenu.popup(QCursor::pos());
        return;
    }

    DockItem::mousePressEvent(e);
}

void AppItem::mouseReleaseEvent(QMouseEvent *e)
{
    m_popupWidget->hide();

    if (e->button() == Qt::LeftButton) {
        AppWindowManager::instance()->clicked(m_entry);
    }
}

void AppItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    refreshIcon();
}

void AppItem::enterEvent(QEvent *e)
{
    DockItem::enterEvent(e);

    m_popupTimer->start();
}

void AppItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);

    m_popupTimer->stop();
    m_popupWidget->setVisible(false);
}