#pragma once

#include <QAbstractItemModel>

//#include <sys/types.h>
//#include <dirent.h>
//#include <stdio.h>


class TreeItem;


class TreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	TreeModel();

	QVariant headerData(int section, Qt::Orientation orientation, int role) const final;

	QModelIndex index(int row, int column, const QModelIndex& _index) const final;
	QModelIndex parent(const QModelIndex& _index) const final;

	int columnCount(const QModelIndex& _index) const final { trace(__FUNCTION__, _index); return 2; }
	int rowCount(const QModelIndex& _index) const final;
	bool hasChildren(const QModelIndex& _index) const final { trace(__FUNCTION__, _index); return rowCount(_index) > 0; }

	QVariant data(const QModelIndex& _index, int _role) const final;

private:
	static void fill(const std::string& _path, TreeItem& _parent, int _level);

	static const TreeItem* treeItem(const QModelIndex& _index)
	{
		return static_cast<const TreeItem*>(_index.internalPointer());
	}

	static void trace(std::string_view _function, const QModelIndex& _index)
	{
		//qDebug("%s: %d %d %p", _function.data(), _index.row(), _index.column(), _index.internalPointer());
	}
	static void trace(std::string_view _function, int _row, int _column)
	{
		//qDebug("%s: %d %d", _function.data(), _row, _column);
	}

private:
	std::shared_ptr<TreeItem> m_root;
};
