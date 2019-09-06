#pragma once

#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <vector>
#include <memory>


/// Add a set of absolute file paths and
/// later check, if a given path is a sub-
/// or parent path of one of those.
/// No filesystem-activity involved!
/// Please make sure the paths are clean beforehand
/// ( no //, no traling /, no relative paths ../../ etc.)
class PathTree
{
private:
    struct _Dir;
    typedef std::shared_ptr<_Dir> _DirPtr;
    typedef std::unordered_map<std::string, _DirPtr > _DirMap;

    struct _Dir {
        _Dir() : isEnd(false){}

        _DirMap children;
        std::weak_ptr<_Dir> parent; // break reference cycles !
        bool isEnd;
        std::string name;
    };

public:
    typedef std::unordered_set<std::string> UnorderdPaths;

    class iterator
    {
    public:
        bool operator==(const iterator& rhs) const;
        bool operator!=(const iterator& rhs) const;

        iterator& operator++ ();
        std::string & operator*() { return d->currentPath; }

    private:
        struct CurrentDirInfo {
            _DirMap::const_iterator it; // current position
            _DirMap::const_iterator end;
            size_t sizeDirName; // putting brace {} here makes compilation fail. Why?
        };

        typedef std::vector<CurrentDirInfo> DirStack;

        iterator(_DirMap::const_iterator begin, _DirMap::const_iterator end,
                 const std::string& path);
        iterator();

        void nextDir();
        bool cdSubDirIfExist(_DirMap::iterator begin, _DirMap::iterator end);
        void nextEntryInParentDirs();
        bool nextSiblingIfExist(CurrentDirInfo& dirInfo);

        void appendPath(const std::string& dirname);
        void stripPath(size_t lastDirSize);

        struct PrivateData{
            DirStack dirStack; // last element always points to the current
                               // dir of iteration
            std::string currentPath;
        };
        std::shared_ptr<PrivateData> d;


        friend class PathTree;
    };

public:

    const iterator begin() const;
    const iterator end() const ;
    iterator iter(const std::string& path) const;
    iterator subpathIter(const std::string& path) const;
    iterator erase(iterator it);

public:
    PathTree();
    ~PathTree();
    PathTree(const PathTree&);
    PathTree& operator=( const PathTree& );


    void clear();
    bool isEmpty() const;

    void insert(const std::string & path);
    template<typename Iterator>
    void insert(Iterator first, Iterator last);

    bool contains(const std::string & path) const;

    bool isParentPath(const std::string& path, bool allowEquals=false) const ;

    bool isSubPath(const std::string& path, bool allowEquals=false) const;

    void printDbg();


private:
    void commonConstructor();

    _DirPtr m_rootDir;
    _DirMap m_rootDirMapDummy;
    static const char sep = '/';

    void printRec(const _DirPtr &node, const std::string& dir="") const;

    _DirPtr mkDirIfNotExist(_DirPtr& parent, const std::string& name);

    _DirPtr findDir(const std::string &path) const;

    static void recursiveCopy(_DirPtr &dst, const _DirPtr &src);    
    static void recursiveClear(_DirPtr &dir);
};


template<typename Iterator>
void PathTree::insert(Iterator first, Iterator last)
{
    for(Iterator it=first; it != last; ++it) {
        this->insert(*it);
    }
}



