#pragma once

#include "ExceptionList.h"
#include <QAbstractTableModel>

namespace Material
{

class ExceptionModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColumnEnabled = 0,
        ColumnType,
        ColumnRegExp,
        nColumns
    };

    explicit ExceptionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void set(const InternalSettingsList &list);
    InternalSettingsList get() const { return m_exceptions; }

    InternalSettingsPtr get(const QModelIndex &index) const;
    InternalSettingsList get(const QModelIndexList &indexes) const;

    void add(InternalSettingsPtr exception);
    void remove(const InternalSettingsList &exceptions);

    bool contains(const QModelIndex &index) const;
    QModelIndex index(InternalSettingsPtr exception) const;

private:
    InternalSettingsList m_exceptions;
};

} // namespace Material
