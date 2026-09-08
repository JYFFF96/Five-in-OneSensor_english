#ifndef POINTDATAPAGEMODEL_H
#define POINTDATAPAGEMODEL_H

#include <QAbstractTableModel>
#include "common.h"  // 替换为你的 PointData 定义头文件

class PointDataPageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PointDataPageModel(QObject *parent = nullptr);

    void addPage(const QList<PointData> &newPage);
    void setCurrentPage(int page);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int pageCount() const { return pages.size(); }
    int currentPageIndex() const { return currentPage; }
    void addData(const QList<PointData> &newData);
private:
    QList<QList<PointData>> pages;
    int pageSize = 16000;
    int maxPages = 100;
    int currentPage = 0;

    QList<PointData> currentBuffer;
};

#endif // POINTDATAPAGEMODEL_H
