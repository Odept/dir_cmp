#include "treemodel.h"

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>


// ========================== DirIterator =====================================
class DirIterator
{
public:
	class dir_open_exception : public std::runtime_error
	{
	public:
		dir_open_exception(const std::string& _what): std::runtime_error(_what) {}
	};

public:
	DirIterator(std::string_view _path):
		m_dir_stream_ptr{opendir(_path.data())}
	{
		if (!m_dir_stream_ptr)
		{
			auto message = std::string{"Failed to open directory \""} + _path.data() + "\"";
			throw dir_open_exception(message);
		}
		m_cur_entry = readdir(m_dir_stream_ptr);
	}
	~DirIterator() { closedir(m_dir_stream_ptr); }

	operator bool() const { return m_cur_entry; }
	const char* name() const { return m_cur_entry ? m_cur_entry->d_name : nullptr; }
	bool is_directory() const { return m_cur_entry ? (m_cur_entry->d_type == DT_DIR) : false; }
	DirIterator& operator++()
	{
		if (m_cur_entry)
			m_cur_entry = readdir(m_dir_stream_ptr);
		return *this;
	}

private:
	DIR* m_dir_stream_ptr;
	dirent* m_cur_entry {};
};


// ========================== TreeItem ========================================
class TreeItem : public std::enable_shared_from_this<TreeItem>
{
public:
	TreeItem(const std::string& _text): m_text{_text} {}

	TreeItem* parent() const { return m_parent.get(); }
	int index() const { return static_cast<int>(m_index); }
	const std::string& text() const { return m_text; }

	int children_count() const { return static_cast<int>(m_children.size()); }
	TreeItem& child(int _index) const { return *m_children[static_cast<size_t>(_index)].get(); }
	auto children() { return m_children; }

	TreeItem& add_child(const std::string& _text)
	{
		m_children.emplace_back(std::make_shared<TreeItem>(shared_from_this(), m_children.size(), _text));
		return **m_children.rbegin();
	}

//private:
//	template<class _Tp, class _A0, class _A1, class _A2>
//	friend std::make_shared;
	//TreeItem(std::shared_ptr<TreeItem> _node):
	TreeItem(std::shared_ptr<TreeItem> _parent, size_t _index, const std::string& _text):
		m_parent{std::move(_parent)},
		m_index{_index},
		m_text{_text}
	{}

private:
	std::shared_ptr<TreeItem> m_parent;
	std::vector<std::shared_ptr<TreeItem>> m_children;
	size_t m_index {};

	std::string m_text;
};


// ========================== TreeModel =======================================
TreeModel::TreeModel()
{
	m_root = std::make_shared<TreeItem>("/Users/user/Desktop");
	fill(m_root->text(), *m_root, 0);
}


void TreeModel::fill(const std::string& _path, TreeItem& _parent, int _level)
{
	if (_level > 1)
		return;

	for (DirIterator it{_path}; it; ++it)
	{
		std::string file_name = it.name();
		if (file_name == "." || file_name == "..")
			continue;

		TreeItem& node = _parent.add_child(file_name);
		if (it.is_directory())
			fill(_path + "/" + file_name, node, _level + 1);
	}
	/*for (auto&& node: _parent.children())
	{
		fill(_path + "/" + node->text(), *node, _level + 1);
	}*/
}


QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		if (section)
			return "%";
		else
			return m_root->text().c_str();
	}
	else
		return QAbstractItemModel::headerData(section, orientation, role);
}


QModelIndex TreeModel::index(int row, int column, const QModelIndex& _index) const
{
	trace(__FUNCTION__, row, column);

	const TreeItem* node_ptr;
	if (_index.isValid())
		node_ptr = treeItem(_index);
	else
		node_ptr = m_root.get();
	return createIndex(row, column, &node_ptr->child(row));
}


QModelIndex TreeModel::parent(const QModelIndex& _index) const
{
	trace(__FUNCTION__, _index);

	if (!_index.isValid())
		return QModelIndex();

	auto parent_ptr = treeItem(_index)->parent();
	if (parent_ptr == m_root.get())
		return QModelIndex();

	return createIndex(parent_ptr->index(), 0, parent_ptr);
}


int TreeModel::rowCount(const QModelIndex& _index) const
{
	trace(__FUNCTION__, _index);

	if (_index.isValid())
		return static_cast<int>(treeItem(_index)->children_count());
	else
		return m_root->children_count();
	;
}


QVariant TreeModel::data(const QModelIndex& _index, int _role) const
{
	if (!_index.isValid())
		return QVariant{};
	if (_role != Qt::DisplayRole)
		return QVariant{};

	trace(__FUNCTION__, _index);
	//qDebug("\t%s", treeItem(_index)->text().c_str());

	return _index.column() ? "100" : QString{treeItem(_index)->text().c_str()};
}
