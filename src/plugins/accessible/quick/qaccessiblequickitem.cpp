/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaccessiblequickitem.h"
#include "QtQuick/private/qquickitem_p.h"
#include "QtQuick/private/qquicktext_p.h"
#include "QtQuick/private/qquickaccessibleattached_p.h"

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

QAccessibleQuickItem::QAccessibleQuickItem(QQuickItem *item)
    : QQmlAccessible(item)
{
}

int QAccessibleQuickItem::childCount() const
{
    return childItems().count();
}

QRect QAccessibleQuickItem::rect() const
{
    const QRect r = itemScreenRect(item());

    if (!r.isValid()) {
        qWarning() << item()->metaObject()->className() << item()->property("accessibleText") << r;
    }
    return r;
}

QRect QAccessibleQuickItem::viewRect() const
{
    // ### no window in some cases.
    if (!item()->window()) {
        return QRect();
    }

    QQuickWindow *window = item()->window();
    QPoint screenPos = window->mapToGlobal(QPoint(0,0));
    return QRect(screenPos, window->size());
}


bool QAccessibleQuickItem::clipsChildren() const
{
    return static_cast<QQuickItem *>(item())->clip();
}


QAccessibleInterface *QAccessibleQuickItem::parent() const
{
    QQuickItem *parent = item()->parentItem();
    if (parent) {
        QQuickWindow *window = item()->window();
        // Jump out to the scene widget if the parent is the root item.
        // There are two root items, QQuickWindow::rootItem and
        // QQuickView::declarativeRoot. The former is the true root item,
        // but is not a part of the accessibility tree. Check if we hit
        // it here and return an interface for the scene instead.
        if (window && (parent == window->contentItem())) {
            return QAccessible::queryAccessibleInterface(window);
        } else {
            return QAccessible::queryAccessibleInterface(parent);
        }
    }
    return 0;
}

QAccessibleInterface *QAccessibleQuickItem::child(int index) const
{
    QList<QQuickItem *> children = childItems();

    if (index < 0 || index >= children.count())
        return 0;

    QQuickItem *child = children.at(index);
    if (!child) // FIXME can this happen?
        return 0;

    return QAccessible::queryAccessibleInterface(child);
}

int QAccessibleQuickItem::indexOfChild(const QAccessibleInterface *iface) const
{
    QList<QQuickItem*> kids = childItems();
    return kids.indexOf(static_cast<QQuickItem*>(iface->object()));
}

QList<QQuickItem *> QAccessibleQuickItem::childItems() const
{
    if (    role() == QAccessible::Button ||
            role() == QAccessible::CheckBox ||
            role() == QAccessible::RadioButton ||
            role() == QAccessible::SpinBox ||
            role() == QAccessible::EditableText ||
            role() == QAccessible::Slider ||
            role() == QAccessible::PageTab ||
            role() == QAccessible::ProgressBar)
        return QList<QQuickItem *>();

    QList<QQuickItem *> items;
    Q_FOREACH (QQuickItem *child, item()->childItems()) {
        QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(child);
        if (itemPrivate->isAccessible)
            items.append(child);
    }
    return items;
}

QAccessible::State QAccessibleQuickItem::state() const
{
    QAccessible::State state;

    if (item()->hasActiveFocus())
        state.focused = true;

    if (!item()->window() ||!item()->isVisible() || qFuzzyIsNull(item()->opacity()))
        state.invisible = true;

    QAccessible::Role r = role();
    switch (r) {
    case QAccessible::Button: {
        QVariant checkable = item()->property("checkable");
        if (!checkable.toBool())
            break;
        // fall through
    }
    case QAccessible::CheckBox:
    case QAccessible::RadioButton: {
        state.checkable = true;
        state.checked = item()->property("checked").toBool();
        break;
    }
    default:
        break;
    }

    return state;
}

QAccessible::Role QAccessibleQuickItem::role() const
{
    // Workaround for setAccessibleRole() not working for
    // Text items. Text items are special since they are defined
    // entirely from C++ (setting the role from QML works.)
    if (qobject_cast<QQuickText*>(const_cast<QQuickItem *>(item())))
        return QAccessible::StaticText;

    QVariant v = QQuickAccessibleAttached::property(item(), "role");
    bool ok;
    QAccessible::Role role = (QAccessible::Role)v.toInt(&ok);
    if (!ok)    // Not sure if this check is needed.
        role = QAccessible::Pane;
    return role;
}

bool QAccessibleQuickItem::isAccessible() const
{
    return item()->d_func()->isAccessible;
}

QString QAccessibleQuickItem::text(QAccessible::Text textType) const
{
    // handles generic behavior not specific to an item
    switch (textType) {
    case QAccessible::Name: {
        QVariant accessibleName = QQuickAccessibleAttached::property(object(), "name");
        if (!accessibleName.isNull())
            return accessibleName.toString();
        break;}
    case QAccessible::Description: {
        QVariant accessibleDecription = QQuickAccessibleAttached::property(object(), "description");
        if (!accessibleDecription.isNull())
            return accessibleDecription.toString();
        break;}
#ifdef Q_ACCESSIBLE_QUICK_ITEM_ENABLE_DEBUG_DESCRIPTION
    case QAccessible::DebugDescription: {
        QString debugString;
        debugString = QString::fromLatin1(object()->metaObject()->className()) + QLatin1Char(' ');
        debugString += isAccessible() ? QLatin1String("enabled") : QLatin1String("disabled");
        return debugString;
        break; }
#endif
    case QAccessible::Value:
    case QAccessible::Help:
    case QAccessible::Accelerator:
    default:
        break;
    }

    // the following blocks handles item-specific behavior
    if (role() == QAccessible::EditableText) {
        if (textType == QAccessible::Value) {
            QVariant text = object()->property("text");
            return text.toString();
        } else if (textType == QAccessible::Name) {
            return object()->objectName();
        }
    }

    return QString();
}

void *QAccessibleQuickItemValueInterface::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::ValueInterface)
       return static_cast<QAccessibleValueInterface*>(this);
    return QAccessibleQuickItem::interface_cast(t);
}

QVariant QAccessibleQuickItemValueInterface::currentValue() const
{
    return item()->property("value");
}

void QAccessibleQuickItemValueInterface::setCurrentValue(const QVariant &value)
{
    item()->setProperty("value", value);
}

QVariant QAccessibleQuickItemValueInterface::maximumValue() const
{
    return item()->property("maximumValue");
}

QVariant QAccessibleQuickItemValueInterface::minimumValue() const
{
    return item()->property("minimumValue");
}

QVariant QAccessibleQuickItemValueInterface::minimumStepSize() const
{
    return item()->property("stepSize");
}

/*!
  \internal
  Shared between QAccessibleQuickItem and QAccessibleQuickView
*/
QRect itemScreenRect(QQuickItem *item)
{
    // ### no window in some cases.
    // ### Should we really check for 0 opacity?
    if (!item->window() ||!item->isVisible() || qFuzzyIsNull(item->opacity())) {
        return QRect();
    }

    QSize itemSize((int)item->width(), (int)item->height());
    // ### If the bounding rect fails, we first try the implicit size, then we go for the
    // parent size. WE MIGHT HAVE TO REVISIT THESE FALLBACKS.
    if (itemSize.isEmpty()) {
        itemSize = QSize((int)item->implicitWidth(), (int)item->implicitHeight());
        if (itemSize.isEmpty() && item->parentItem())
            // ### Seems that the above fallback is not enough, fallback to use the parent size...
            itemSize = QSize((int)item->parentItem()->width(), (int)item->parentItem()->height());
    }

    QPointF scenePoint = item->mapToScene(QPointF(0, 0));
    QPoint screenPos = item->window()->mapToGlobal(scenePoint.toPoint());
    return QRect(screenPos, itemSize);
}

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE
