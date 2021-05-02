#pragma once

#include <vector>
#include <memory>
#include <unordered_set>
#include <QHash>


#include "strlight.h"


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
    typedef QHash<StrLight, _DirPtr > _DirMap;

    struct _Dir {
        _Dir(const StrLight& name) :
            isEnd(false),
            name(name){}

        _DirMap children;
        std::weak_ptr<_Dir> parent; // break reference cycles !
        bool isEnd;
        StrLight name;
    };

public:

    class iterator
    {
    public:
        bool operator==(const iterator& rhs) const;
        bool operator!=(const iterator& rhs) const;

        iterator& operator++ ();
        StrLight & operator*() { return d->currentPath; }

    private:
        struct CurrentDirInfo {
            _DirMap::const_iterator it; // current position
            _DirMap::const_iterator end;
            size_t sizeDirName; // putting brace {} here makes compilation fail. Why?
        };

        typedef std::vector<CurrentDirInfo> DirStack;

        iterator(_DirMap::const_iterator begin, _DirMap::const_iterator end,
                 const StrLight &path);
        iterator();

        void nextDir();
        bool cdSubDirIfExist(_DirMap::iterator begin, _DirMap::iterator end);
        void nextEntryInParentDirs();
        bool nextSiblingIfExist(CurrentDirInfo& dirInfo);

        void appendPath(const StrLight& dirname);
        void stripPath(size_t lastDirSize);

        struct PrivateData{
            DirStack dirStack; // last element always points to the current
                               // dir of iteration
            StrLight currentPath;
        };
        std::shared_ptr<PrivateData> d;

        friend class PathTree;
    };

public:

    const iterator begin() const;
    const iterator end() const ;

    iterator iter(const StrLight& path) const;
    iterator subpathIter(const StrLight& path) const;
    iterator erase(iterator it);

public:
    PathTree();
    ~PathTree() = default;
    PathTree(const PathTree&) = delete ;
    PathTree& operator=( const PathTree& ) = delete ;

    void clear();
    bool isEmpty() const;

    void insert(const StrLight& path);
    template<typename Iterator>
    void insert(Iterator first, Iterator last);

    bool contains(const StrLight & path) const;

    bool isParentPath(const StrLight &path, bool allowEquals=false) const ;

    bool isSubPath(const StrLight &path, bool allowEquals=false) const;

    void printDbg();


    const std::unordered_set<StrLight>& allPaths() const;

private:
    static const char sep = '/';

    void commonConstructor();

    _DirPtr m_rootDir;
    _DirMap m_rootDirMapDummy;
    mutable StrLight m_rawbuftmp;
    std::unordered_set<StrLight> m_allPaths;
    std::vector<size_t> m_orderedPathlenghts;
    bool m_rootNodeIsContained;


    void printRec(const _DirPtr &node, const StrLight &dir="") const;

    _DirPtr mkDirIfNotExist(_DirPtr& parent, const StrLight &name);

    _DirPtr findDir(const StrLight &path) const;

    // static void recursiveCopy(_DirPtr &dst, const _DirPtr &src);
    static void recursiveClear(_DirPtr &dir);
};


template<typename Iterator>
void PathTree::insert(Iterator first, Iterator last)
{
    for(Iterator it=first; it != last; ++it) {
        this->insert(*it);
    }
}



