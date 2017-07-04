/*
 * Copyright (C) 2017 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file elementcontainermodel.h
/// The table model for elements.

#ifndef ELEMENTCONTAINERMODEL_H
#define ELEMENTCONTAINERMODEL_H

#include <unordered_map>
#include <vector>

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

#include "src/element.h"
#include "src/event.h"

#include "model.h"

namespace scram {
namespace gui {
namespace model {

/// The model to list elements in a table.
class ElementContainerModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    int rowCount(const QModelIndex &parent) const final;

protected:
    /// @tparam T  The container of smart pointers to elements.
    template <typename T>
    explicit ElementContainerModel(const T &container,
                                   QObject *parent = nullptr);

    /// Puts the element pointer into the index's internal pointer.
    QModelIndex index(int row, int column,
                      const QModelIndex &parent) const override;

    /// @returns The element with the given index (row).
    ///
    /// @pre The index is valid.
    Element *getElement(int index) const;

    /// @returns The current index (row) of the element.
    ///
    /// @pre The element is in the table.
    int getElementIndex(Element *element) const;

    void addElement(Element *element);
    void removeElement(Element *element);

    const std::vector<Element *> elements() const { return m_elements; }

private:
    /// Connects of the element change signals to the table modification.
    virtual void connectElement(Element *element) = 0;

    std::vector<Element *> m_elements;
    std::unordered_map<Element *, int> m_elementToIndex;
};

/// The proxy model allows sorting and filtering.
class SortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    /// Keep the row indices sequential.
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override
    {
        return sourceModel()->headerData(section, orientation, role);
    }
};

class BasicEventContainerModel : public ElementContainerModel
{
    Q_OBJECT

public:
    using ItemModel = BasicEvent;
    using DataType = mef::BasicEvent;

    explicit BasicEventContainerModel(Model *model, QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent) const override;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    QVariant data(const QModelIndex &index, int role) const override;

private:
    void connectElement(Element *) final {} ///< @todo Track event changes.
};

class HouseEventContainerModel : public ElementContainerModel
{
    Q_OBJECT

public:
    using ItemModel = HouseEvent;
    using DataType = mef::HouseEvent;

    explicit HouseEventContainerModel(Model *model, QObject *parent = nullptr);

    int columnCount(const QModelIndex &parent) const override;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    QVariant data(const QModelIndex &index, int role) const override;

private:
    void connectElement(Element *element) final;
};

} // namespace model
} // namespace gui
} // namespace scram

#endif // ELEMENTCONTAINERMODEL_H