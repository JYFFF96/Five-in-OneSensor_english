#include "PointDataPageModel.h"

PointDataPageModel::PointDataPageModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void PointDataPageModel::addPage(const QList<PointData> &newPage)
{
    // 保证页大小一致
    QList<PointData> page = newPage;
    if (page.size() > pageSize)
        page = page.mid(0, pageSize);

    // 加入新页
    pages.append(page);

    // 超出最大页数，清理最老一页
    while (pages.size() > maxPages) {
        pages.removeFirst();
    }

    // 自动切换到最后一页
    currentPage = pages.size() - 1;

    beginResetModel();
    endResetModel();
}

void PointDataPageModel::setCurrentPage(int page)
{
    if (page < 0 || page >= pages.size() || page == currentPage)
        return;

    currentPage = page;
    beginResetModel();
    endResetModel();
}

int PointDataPageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (pages.isEmpty())
        return 0;
    return pages[currentPage].size();
}

int PointDataPageModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 9;
}

QVariant PointDataPageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    if (pages.isEmpty())
        return QVariant();

    const QList<PointData> &page = pages[currentPage];
    if (index.row() >= page.size())
        return QVariant();

    const PointData &point = page[index.row()];

    switch (index.column()) {
    case 0: return index.row() + 1;
    case 1: return QString::number(point.time, 'f', 6);
    case 2: return point.channel;
    case 3: return point.distance;
    case 4: return point.azimuthValue;
    case 5: return point.intensity;
    case 6: return point.x;
    case 7: return point.y;
    case 8: return point.z;
    default: return QVariant();
    }
}

QVariant PointDataPageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return QStringLiteral("No.");
        case 1: return QStringLiteral("Timestamp");
        case 2: return QStringLiteral("Channel");
        case 3: return QStringLiteral("Distance");
        case 4: return QStringLiteral("Azimuth");
        case 5: return QStringLiteral("Reflectivity");
        case 6: return QStringLiteral("XCoordinates");
        case 7: return QStringLiteral("YCoordinates");
        case 8: return QStringLiteral("ZCoordinates");
        default: return QVariant();
        }
    } else {
        return section + 1;
    }
}

void PointDataPageModel::addData(const QList<PointData> &newData)
{
    currentBuffer.append(newData);

    while (currentBuffer.size() >= pageSize) {
        // 取出前 pageSize 行，作为一页
        QList<PointData> page = currentBuffer.mid(0, pageSize);
        pages.append(page);

        // 移除已添加的数据
        currentBuffer.erase(currentBuffer.begin(), currentBuffer.begin() + pageSize);

        // 超出最大页，移除最老页
        while (pages.size() > maxPages) {
            pages.removeFirst();
        }

        // 自动切换到最后一页
        currentPage = pages.size() - 1;

        // 刷新表
        beginResetModel();
        endResetModel();
    }
}
