#include <iostream>
#include <random>
#include <stack>
#include <unordered_map>
#include <unordered_set>

#ifdef __APPLE__
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif


using namespace std::literals::string_view_literals;

using range_t = std::pair<unsigned, unsigned>;

constexpr range_t LevelCountLimits {2, 5};
constexpr range_t RecordCountPerLevelLimits {10, 15};
constexpr unsigned PercentOfDirs {25};
constexpr range_t PercentOfExtNames {90/*most common*/, 5/*garbage*/ /*, no ext*/};
constexpr range_t FileNameLengthLimits {3, 8};
constexpr range_t ChangesLimits {3, 7};
constexpr unsigned PercentOfObjectsToTest {5};


namespace
{
	std::ostream& operator<<(std::ostream& _os, const range_t& _range)
	{
		_os << '[' << _range.first << ", " << _range.second << ']';
		return _os;
	}
	void log_msg() { std::cout << std::endl; }
	template<typename Arg, typename... Args>
	void log_msg(Arg&& _arg, Args&&... _args)
	{
		std::cout << std::forward<Arg>(_arg);
		log_msg(std::forward<Args>(_args)...);
	}

	std::string join_path(std::string _base) { return _base; }
	template<typename Arg, typename... Args>
	std::string join_path(std::string _base, Arg _arg, Args... _args)
	{
		assert(_base.back() != '/');
		assert(_arg.front() != '/');
		_base += '/' + _arg;
		return join_path(std::move(_base), _args...);
	}
}


class PRNG
{
	std::mt19937 m_prng;

public:
	PRNG(typename decltype(m_prng)::result_type _seed): m_prng(_seed) {}

	unsigned operator()(unsigned _limit)
	{
		return m_prng() % _limit;
	}

	unsigned operator()(const range_t& _limits)
	{
		assert(_limits.first <= _limits.second);
		return _limits.first + m_prng() % (_limits.second - _limits.first + 1);
	}

	std::string str(const range_t& _len_limits)
	{
		std::vector<char> chars((*this)(_len_limits));
		for (auto&& c: chars)
			c = character();
		return std::string(chars.begin(), chars.end());
	}
	std::string str(unsigned _len) { return str({_len, _len}); }

private:
	char character()
	{
		constexpr unsigned MaxChars = 10 + ('z' - 'a' + 1) * 2;
		unsigned selector = (*this)(MaxChars);
		if (selector < 10)
			return selector + '0';
		else if (constexpr char min_c = 10; selector < (min_c + 'z' - 'a' + 1))
			return selector - min_c + 'a';
		else if (constexpr char min_c = 10 + 'z' - 'a' + 1)
			return selector - min_c + 'A';
	}
};
static PRNG s_prng(31337);


// ============================================================================
template<class T>
class TreeNode
{
public:
	TreeNode(unsigned _level):
		m_is_leaf{_level ? (s_prng(100) >= PercentOfDirs) : false}
	{
		if (m_is_leaf)
			return;

		if (_level < s_prng(LevelCountLimits))
		{
			for (unsigned i = 0, c = s_prng(RecordCountPerLevelLimits); i < c; ++i)
				m_children.emplace_back(std::make_unique<T>(_level + 1));
		}
	}

	explicit TreeNode(const TreeNode& _rhs):
		m_is_leaf{_rhs.m_is_leaf}
	{
		for (auto&& node: _rhs.m_children)
			m_children.emplace_back(std::make_unique<T>(*node));
	}

protected:
	bool m_is_leaf; // TODO: to std::optional<std::vector...>?
	std::vector<std::unique_ptr<T>> m_children;
};


class FSNode : public TreeNode<FSNode>
{
	template<bool> friend class FSTreeIterator;

public:
	explicit FSNode(const FSNode&) = default;
	FSNode& operator=(const FSNode&) = delete;

//private:
	FSNode(unsigned _level):
		TreeNode{_level},
		m_name{s_prng.str(FileNameLengthLimits)}
	{
		if (is_dir())
			return;

		unsigned percent = s_prng(100);
		if (percent < PercentOfExtNames.first)
			m_name += ".mp3";
		else if (percent < (PercentOfExtNames.first + PercentOfExtNames.second))
			m_name += '.' + s_prng.str(3);
	}

	const std::string& name() const { return m_name; }
	void set_name(const std::string& _name) { m_name = _name; }

	unsigned count_dirs() const { return count_nodes<true>(); }
	unsigned count_files() const { return count_nodes<false>(); }

	void remove(const FSNode* _node)
	{
		for (auto it = m_children.begin(), end = m_children.end(); it != end; ++it)
		{
			if (it->get() == _node)
			{
				m_children.erase(it);
				return;
			}
		}
		throw std::logic_error("Failed to delete node \"" + _node->name() + "\" from \"" + name() + '\"');
	}

protected:
	bool is_dir() const { return !m_is_leaf; }

	void materialize(const std::string _parent_path) const
	{
		std::string path {join_path(_parent_path, m_name)};
		if (is_dir())
		{
			//log_msg("Creating dir \"", path, '\"');
			if (mkdir(path.c_str(), m_dir_permissions))
				throw std::runtime_error{"Failed to create directory \"" + path + "\" (" + std::strerror(errno) + ')'};
			for (auto&& node: m_children)
				node->materialize(path);
		}
		else
		{
			assert(m_children.empty());
			//log_msg("Creating file \"", path, '\"');
			int fd = open(path.c_str(), m_file_creation_mode);
			if (fd < 0)
				throw std::runtime_error{"Failed to create file \"" + path + "\" (" + std::strerror(errno) + ')'};
			close(fd);
		}
	}

private:
	template<bool IsDir>
	unsigned count_nodes() const
	{
		unsigned c = is_dir() ^ !IsDir;
		for (auto&& node: m_children)
			c += node->count_nodes<IsDir>();
		return c;
	}

protected:
	std::string m_name;

private:
	static constexpr mode_t m_dir_permissions {0777};
	static constexpr mode_t m_file_creation_mode {O_CREAT | O_EXCL | O_RDONLY};
};


// ============================================================================
template<bool IsDir>
class FSTreeIterator
{
public:
	FSTreeIterator() = default;
	FSTreeIterator(/*FSTree*/FSNode& _tree)
	{
		m_nodes.push(&_tree);
		m_parents_map[&_tree] = nullptr;
		verify();
	}
	
	FSTreeIterator& operator++() { next(); return *this; }
	FSNode& operator*() { return *m_nodes.top(); }
	FSNode* operator->() { return m_nodes.top(); }
	bool operator!=(const FSTreeIterator& _rhs) const { return (m_nodes != _rhs.m_nodes); }
	
	auto hierarchy()
	{
		std::stack<FSNode*> path_stack;
		for (FSNode* ptr = operator->(); ptr; ptr = m_parents_map.find(ptr)->second)
			path_stack.push(ptr);
		return path_stack;
	}
	std::string path()
	{
		std::string rel_path;
		for (auto path_stack = hierarchy(); !path_stack.empty(); path_stack.pop())
			rel_path += '/' + path_stack.top()->name();
		return rel_path;
	}

	static unsigned count(/*FSTree*/FSNode& _tree)
	{
		if constexpr (IsDir)
			return _tree.count_dirs();
		else
			return _tree.count_files();
	}

private:
	void next()
	{
		FSNode* cur = m_nodes.top();
		m_nodes.pop();

		for (auto&& node: cur->m_children)
		{
			if constexpr (IsDir)
			{
				if (!cur->is_dir())
					continue;
			}
			m_nodes.push(node.get());
			m_parents_map[node.get()] = cur;
		}
		if (!m_nodes.empty())
			verify();
	}
	void verify()
	{
		if (IsDir ^ m_nodes.top()->is_dir())
			next();
	}

private:
	std::stack<FSNode*> m_nodes;
	std::unordered_map<FSNode*, FSNode*> m_parents_map;
};
using DirIterator = FSTreeIterator<true>;
using FileIterator = FSTreeIterator<false>;


class FSTree : public FSNode
{
public:
	template<typename T>
	class IteratorGenerator
	{
		T m_begin;
		unsigned m_count;
	public:
		IteratorGenerator(FSTree& _tree): m_begin{_tree}, m_count{T::count(_tree)} {}
		T begin() { return m_begin; }
		T end() { return T{}; }
		size_t size() const { return m_count; }
	};
	using DirTree = IteratorGenerator<DirIterator>;
	using FileTree = IteratorGenerator<FileIterator>;

public:
	explicit FSTree(): FSNode{0} {}
	explicit FSTree(const FSTree&) = default;

	void materialize() const
	{
		log_msg("Creating \"", path(), '\"');
		assert(is_dir());
		FSNode::materialize(m_root_path);
	}

	std::string path() const { return join_path(m_root_path, m_name); }

	DirTree dirs() { return DirTree{*this}; }
	FileTree files() { return FileTree{*this}; }

private:
	static const std::string m_root_path;
};
const std::string FSTree::m_root_path
{
#ifdef __APPLE__
	"/tmp"
#else // _WIN32
	static_assert(false, "Unsupported OS");
#endif
};


// ============================================================================
class FSTreeClone
{
public:
	std::string original() const { return m_original_dir.path(); }
	std::string cloned() const { return m_cloned_dir.path(); }

	void operator()() const
	{
		m_original_dir.materialize();
		m_cloned_dir.materialize();
	}

protected:
	FSTreeClone(): m_original_dir{}, m_cloned_dir{m_original_dir} { m_cloned_dir.set_name(m_cloned_dir.name() + '2'); }

	static unsigned objects_to_test(unsigned _total_objects_count)
	{
		unsigned c = _total_objects_count * PercentOfObjectsToTest / 100;
		return c ? c : 1;
	}

private:
	friend class FSTreeCloneDebug;
	FSTree m_original_dir;
protected:
	FSTree m_cloned_dir;
};

class FSTreeCloneDebug : public FSTreeClone
{
public:
	FSTreeCloneDebug(): FSTreeClone{}
	{
		log_msg(__FUNCTION__, " (", m_original_dir.name(), ')');
		{
			log_msg("Dirs (", m_original_dir.count_dirs(), ") :");
			unsigned i {};
			auto dirs = m_original_dir.dirs();
			for (auto it = dirs.begin(), end = dirs.end(); it != end; ++it)
				log_msg("\t ", i++, ": ", it->name(), " (", it.path(), ')');
		}
		{
			log_msg("Files (", m_original_dir.count_files(), ") :");
			unsigned i {};
			auto files = m_original_dir.files();
			for (auto it = files.begin(), end = files.end(); it != end; ++it)
				log_msg("\t ", i++, ": ", it->name(), " (", it.path(), ')');
		}
	}
};

template<bool IsDir>
class FSTreeCloneDelete : public FSTreeClone
{
	auto objects()
	{
		if constexpr (IsDir)
			return m_cloned_dir.dirs();
		else
			return m_cloned_dir.files();
	}

protected:
	FSTreeCloneDelete(): FSTreeClone{}
	{
		auto fs_objects = objects();

		std::unordered_set<unsigned> delete_at;
		for (unsigned i {}, objects_total = fs_objects.size(), c = IsDir ? 1 : objects_to_test(objects_total); i < c; ++i)
			delete_at.insert(s_prng({1, objects_total - 1}));
		unsigned i {};

		for (auto it = fs_objects.begin(), end = fs_objects.end(); it != end; ++i)
		{
			auto it_delete_at = delete_at.find(i);
			if (it_delete_at == delete_at.end())
			{
				++it;
				continue;
			}

			log_msg("Removing ", IsDir ? "folder" : "file", " #", i, " (", it.path(), ')');
			auto hierarchy = it.hierarchy();
			while (hierarchy.size() > 2)
				hierarchy.pop();
			auto parent = hierarchy.top();
			hierarchy.pop();

			++it;
			parent->remove(hierarchy.top());
			delete_at.erase(it_delete_at);
			if (delete_at.empty())
				return;
		}
		throw std::logic_error(std::to_string(delete_at.size()) + " target " + (IsDir ? "folders" : "files") + " not found");
	}
};

// ====================================
class FSTreeCloneSame : public FSTreeClone {};
class FSTreeCloneCreateFolder : public FSTreeClone
{
/*public:
	FSTreeCloneCreateFolder()
	{
	}*/
};
class FSTreeCloneCreateFile : public FSTreeClone {};
class FSTreeCloneDeleteFolder : public FSTreeCloneDelete<true> {};
class FSTreeCloneDeleteFiles : public FSTreeCloneDelete<false> {};
class FSTreeCloneRenameFolders : public FSTreeClone
{};
class FSTreeCloneRenameFiles : public FSTreeClone
{};
class FSTreeCloneModifyFiles : public FSTreeClone
{};


// ============================================================================
int main(int, const char**)
{
	//FSTreeCloneDebug dirs_debug;
	//FSTreeCloneSame()();
	//FSTreeCloneDeleteFolder dirs_delete_folders; dirs_delete_folders();
	//FSTreeCloneDeleteFiles dirs_delete_files; dirs_delete_files();
	FSTreeCloneRenameFolders dirs_rename_folders; dirs_rename_folders();

	return 0;
}

