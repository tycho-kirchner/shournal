#include <cassert>
#include <iostream>
#include "pathtree.h"
#include "util.h"

/// Construct a new dir iterator pointing at a directory entries at path.
PathTree::iterator::iterator(_DirMap::const_iterator begin, _DirMap::const_iterator end,
                             const std::string& path) :
    d(std::make_shared<PrivateData>())
{
    if(begin == end){
        return;
    }

    d->currentPath = path;
    appendPath(begin->first);
    d->dirStack.push_back( { begin, end, begin->first.size() });

    if(! begin->second->isEnd){
        // move to first *really* inserted path
        ++(*this);
    }
}

PathTree::iterator::iterator()
= default;



bool PathTree::iterator::operator==(const PathTree::iterator &rhs) const
{
    bool ourDirsEmpty = (d == nullptr) ? true : d->dirStack.empty();
    bool otherDirsEmpty = (rhs.d == nullptr) ? true : rhs.d->dirStack.empty();

    if(ourDirsEmpty || otherDirsEmpty){
        // cannot compare dirStack
        return ourDirsEmpty == otherDirsEmpty;
    }
    return d->currentPath == rhs.d->currentPath;
}

bool PathTree::iterator::operator!=(const PathTree::iterator &rhs) const
{
    return ! (*this == rhs);
}

/// Iterate exactly over the inserted directories skipping
/// possible other paths, e.g. if /home/user/foo is set,
/// /home and /home/user will be skipped.
PathTree::iterator &PathTree::iterator::operator++()
{
    assert(! d->dirStack.empty());
    while (true) {
        nextDir();
        if(d->dirStack.empty()){
            return *this;
        }
        auto & currentDir = d->dirStack.back();
        if(currentDir.it != currentDir.end &&
                currentDir.it->second->isEnd){
            return *this;
        }
    }
}


/// go to next dir, prefering going as deep as possible
/// first, then to the sibling directories and finally
/// walk up the tree again, jumping over already visited dirs.
void PathTree::iterator::nextDir()
{
    assert(! d->dirStack.empty());
    auto & dirInfo = d->dirStack.back();
    // go into depth first, if possible
    auto & subDirs = dirInfo.it->second->children;

    if(cdSubDirIfExist(subDirs.begin(), subDirs.end())){
        return;
    }
    if(nextSiblingIfExist(dirInfo)){
        return;
    }
    nextEntryInParentDirs();
}


bool PathTree::iterator::cdSubDirIfExist(_DirMap::iterator begin, _DirMap::iterator end)
{
    if(begin == end){
        return false;
    }
    appendPath(begin->first);
    d->dirStack.push_back( { begin, end, begin->first.size() });
    return true;
}

/// Go up as many parent directories necessary until the next valid
/// entry is found. The dirstack will be empty, in case we're done
void PathTree::iterator::nextEntryInParentDirs()
{
    while (true) {
        auto & currentDirInfo = d->dirStack.back();
        stripPath(currentDirInfo.sizeDirName);
        d->dirStack.pop_back();
        if(d->dirStack.empty()){
            // we are done -> empty stack == iterator.end()
            return;
        }
        auto & upperDir = d->dirStack.back();
        if(nextSiblingIfExist(upperDir)){
            return;
        }
        // got this dir as well -> go up even more
    }
}

/// increment the passed dir and adjust the path appropriately if we could switch
/// to the next sibling (same directory level, no parent- or subdir)
bool PathTree::iterator::nextSiblingIfExist(PathTree::iterator::CurrentDirInfo &dirInfo)
{
    ++dirInfo.it;
    if(dirInfo.it == dirInfo.end){
        return false;
    }
    stripPath(dirInfo.sizeDirName);
    appendPath(dirInfo.it->first);
    dirInfo.sizeDirName = dirInfo.it->first.size();
    return true;
}

void PathTree::iterator::appendPath(const std::string &dirname)
{
    if( d->currentPath.empty() || d->currentPath.back() != '/'){
        d->currentPath += '/';
    }
    d->currentPath += dirname;
}

void PathTree::iterator::stripPath(size_t lastDirSize)
{
    assert(lastDirSize <= d->currentPath.size());
    d->currentPath.resize(d->currentPath.size() - lastDirSize);
    if(! d->currentPath.empty() && d->currentPath.back() == '/'){
        d->currentPath.pop_back();
    }
}


const PathTree::iterator PathTree::begin() const
{
    return iterator(m_rootDirMapDummy.begin(), m_rootDirMapDummy.end(), "/");
}


const PathTree::iterator PathTree::end() const
{
    return iterator();
}

/// @return an iterator pointing on the directory-node corresponding to path.
/// Subsequentially incrementing it results in an iteration of all sub-paths
/// as well as path, if it exists. Note: path may also be an intermediate
/// directory (dir->isEnd == false)
PathTree::iterator PathTree::iter(const std::string &path) const
{
    auto dir =findDir(path);
    if(dir == nullptr  ){
        return end();
    }
    _DirMap::const_iterator itOfChildInParent;
    _DirMap::const_iterator dummyEnd;
    if(dir == m_rootDir){
        itOfChildInParent = m_rootDirMapDummy.begin();
        dummyEnd = m_rootDirMapDummy.end();
    } else {
        // need to determine the iterators from parent.
        auto parentDirWeak = dir->parent;
        assert(! is_uninitialized(parentDirWeak));
        auto parentDir = parentDirWeak.lock();

        itOfChildInParent = parentDir->children.find(dir->name);
        assert(itOfChildInParent != parentDir->children.end());
        // Do not iterate over possible siblings as well: set next one as end()
        // (even if it is no real end)
        dummyEnd = itOfChildInParent;
        ++dummyEnd;
    }
    return iterator(itOfChildInParent, dummyEnd, splitAbsPath(path).first);
}

/// @return: An iterator for all subpaths of param path (so path is *not*
/// traversed).
 PathTree::iterator PathTree::subpathIter(const std::string &path) const
{
    auto dir =findDir(path);
    if(dir == nullptr  ){
        return end();
    }
    return iterator(dir->children.begin(), dir->children.end(), path);
 }

 PathTree::iterator PathTree::erase(PathTree::iterator it)
 {
     assert(! it.d->dirStack.empty());
     assert( it.d->dirStack.back().it != it.d->dirStack.back().end);
     auto dir = it.d->dirStack.back().it->second;
     assert(dir->isEnd);
     dir->isEnd = false;

     // before possibly deleting empty in-between paths (isEnd=false)
     // move to next 'real dir' -> otherwise iterators might have been invalidated.
     ++it;

     // go up the current tree and erase all empty dirs (stop on first non-empty)
     while (true) {
         if(! dir->children.empty()){
             // our dir has children, so do not erase it!
             return it;
         }
         // our dir has no children, so it is safe for our parent to delete it
         auto parentDirWeak = dir->parent;
         if(is_uninitialized(parentDirWeak)){
             // reached root /
             return it;
         }
         auto parentDir = dir->parent.lock();

         auto itOfDirInParent = parentDir->children.find(dir->name);
         assert(itOfDirInParent != parentDir->children.end());
         parentDir->children.erase(itOfDirInParent);
         dir = parentDir;
     }
 }

//////////////////////////////////////////////////////////////////////////////////////////////

PathTree::PathTree()
{
    commonConstructor();
}

PathTree::~PathTree()= default;

PathTree::PathTree(const PathTree &o)
{
    commonConstructor();
    *this = o;
}


void PathTree::commonConstructor()
{
    m_rootDir = std::make_shared<_Dir>();
    m_rootDirMapDummy = {{"", m_rootDir}};
    m_rootDir->name = '/';
}

PathTree &PathTree::operator=(const PathTree & o)
{
    if(this != &o) {
        assert(m_rootDir != nullptr);
        this->clear();
        recursiveCopy(m_rootDir, o.m_rootDir);
    }
    return *this;
}

void PathTree::printDbg()
{
    if(m_rootDir->children.empty()){
        std::cerr << __func__ << " tree is empty\n";
    } else {
        printRec(m_rootDir);
    }
}

void PathTree::clear()
{
    m_rootDir->children.clear();
    m_rootDir->isEnd = false;
}

bool PathTree::isEmpty() const
{
    return m_rootDir->children.empty();
}



void PathTree::insert(const std::string &path){
    assert( path.find("//") == std::string::npos);

    auto currenDir = m_rootDir;
    std::stringstream ss(path);
    std::string dirname;
    while (std::getline(ss, dirname, sep)) {
        if(dirname.empty()){
            continue;
        }
        currenDir = mkDirIfNotExist(currenDir, dirname);
    }
    currenDir->isEnd = true;
}

bool PathTree::contains(const std::string &path) const
{
    // return m_paths.find(path) != m_paths.end();
    auto dir = findDir(path);
    if(dir == nullptr){
        return false;
    }
    return dir->isEnd;
}

/// Check if path is a parent path of any other path within this
/// tree. Example:
/// /home/user/foo exists in this tree and it is queried for path
/// /home/user -> true is returned
///
/// If allowEquals is true, true is also returned, if
/// the searched path is contained but has no children (equals
/// to the searched path).
bool PathTree::isParentPath(const std::string &path, bool allowEquals) const {
    auto dir = findDir(path);
    if(dir == nullptr){
        return false;
    }

    if(! dir->children.empty()){
        return true;
    }
    // no children exist
    return dir->isEnd && allowEquals;
}

///  @return true, if param path is subpath of any previously inserted paths
/// or the same, if allowEquals=true
bool PathTree::isSubPath(const std::string &path, bool allowEquals) const {
    auto node = m_rootDir;
    std::stringstream ss(path);
    std::string dir_;
    while (std::getline(ss, dir_, sep)) {
        if(dir_.empty()){
            continue;
        }
        if(node->isEnd){
            // we already jumped over our parent path
            return true;
        }
        auto it = node->children.find(dir_);
        if(it == node->children.end()){
            return false;
        }
        node = it->second;
    }
    return (node->isEnd && allowEquals);
}

void PathTree::printRec(const PathTree::_DirPtr &node, const std::string &dir) const
{
    for(const auto & n : node->children){
        auto fullPath = dir + "/" + n.first;
        std::cerr << __func__ << ": " << fullPath << "\n" ;
        printRec(n.second, fullPath);
    }
}

/// @return the new or existing dir
PathTree::_DirPtr PathTree::mkDirIfNotExist(PathTree::_DirPtr &parent,
                                            const std::string &name)
{
    auto it = parent->children.find(name);
    if(it == parent->children.end()){
        auto newDir = std::make_shared<_Dir>();
        newDir->parent = parent;
        newDir->name = name;
        parent->children[name] = newDir;
        return newDir;
    }
    return it->second;
}


/// @return The node exactly matching the passed path or nullptr
PathTree::_DirPtr PathTree::findDir(const std::string &path) const
{
    auto currentDir = m_rootDir;
    std::stringstream ss(path);
    std::string dirname;
    while (std::getline(ss, dirname, sep)) {
        if(dirname.empty()){
            continue;
        }
        auto it = currentDir->children.find(dirname);
        if(it == currentDir->children.end()){
            return nullptr;
        }
        currentDir = it->second;
    }
    return currentDir;
}


void PathTree::recursiveCopy(_DirPtr& dst, const _DirPtr& src)
{
    dst->isEnd = src->isEnd;
    dst->name = src->name;
    dst->children.reserve(src->children.size());
    for(auto& subSrc  : src->children){
        auto newDir = std::make_shared<_Dir>();
        dst->children[subSrc.first] = newDir;
        newDir->parent = dst;
        recursiveCopy(newDir, subSrc.second);
    }
}


void PathTree::recursiveClear(PathTree::_DirPtr &dir)
{
    for(auto& sub  : dir->children){
        recursiveClear(sub.second);
    }
    dir->children.clear();
}



