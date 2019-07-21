
#include <QTest>
#include <QDebug>
#include <iostream>

#include "autotest.h"

#include "pathtree.h"
#include "util.h"



class PathTreeTest : public QObject {
    Q_OBJECT

    void checkAllSubPathsExist(PathTree& tree,
                               const std::string& parentPath,
                               std::unordered_set<std::string> paths){
        for(auto treeIt=tree.subpathIter(parentPath); treeIt != tree.end(); ++treeIt){
            auto it = paths.find(*treeIt);
            QVERIFY2(it != paths.end(), (*treeIt).c_str());
            paths.erase(it);
        }
        QVERIFY(paths.empty());
    }

    void checkAllExist(PathTree& tree,
                       std::unordered_set<std::string> paths){
        for(const auto & p : tree){
            auto it = paths.find(p);
            QVERIFY2(it != paths.end(), (p).c_str());
            paths.erase(it);
        }
        std::string p = (paths.empty()) ? "" : *paths.begin();
        QVERIFY2(paths.empty(), p.c_str());
    }

    void erasePathTreeFromIt(PathTree& tree, PathTree::iterator it){
        while(it != tree.end() ){
            it = tree.erase(it);
        }
    }


private slots:

    void testContains(){
        PathTree tree;
        QVERIFY(! tree.contains("/"));
        tree.insert("/");
        QVERIFY(tree.contains("/"));
        tree.clear();

        QVERIFY(! tree.contains("/"));

        tree.insert("/");
        tree.insert("/home/user/foo");

        QVERIFY(tree.contains("/"));
        QVERIFY(tree.contains("/home/user/foo"));

    }


    void testParent() {
        PathTree tree;
        QVERIFY(! tree.isParentPath("/"));
        QVERIFY(! tree.isParentPath("/home"));

        tree.insert("/home/user");

        QVERIFY(tree.isParentPath("/home"));
        QVERIFY(tree.isParentPath("/"));
        QVERIFY(tree.isParentPath("/home/user", true));
        QVERIFY(! tree.isParentPath("/home/user"));
        QVERIFY(! tree.isParentPath("/home/user/foo", true));
        QVERIFY(! tree.isParentPath("/home/user/foo", false));

        // special case root
        PathTree tree2;
        tree2.insert("/");
        QVERIFY(! tree2.isParentPath("/", false));
        QVERIFY( tree2.isParentPath("/", true));
        QVERIFY( ! tree2.isParentPath("/home"));

        tree2.insert("/home/user");
        QVERIFY( tree2.isParentPath("/", false));
        QVERIFY( tree2.isParentPath("/", true));
        QVERIFY( tree2.isParentPath("/home"));
        QVERIFY(! tree2.isParentPath("/home/user", false));
        QVERIFY( tree2.isParentPath("/home/user", true));


    }

    void testSub() {
        PathTree tree;
        QVERIFY(! tree.isSubPath("/"));
        QVERIFY(! tree.isSubPath("/home"));

        tree.insert("/home/user");
        QVERIFY(! tree.isSubPath("/home"));
        QVERIFY(! tree.isSubPath("/home/user", false));
        QVERIFY( tree.isSubPath("/home/user", true));
        QVERIFY( tree.isSubPath("/home/user/foo", false));
        QVERIFY( tree.isSubPath("/home/user/foo", true));

        // special case root
        PathTree tree2;
        tree2.insert("/");
        QVERIFY(! tree2.isSubPath("/", false));
        QVERIFY( tree2.isSubPath("/", true));
        QVERIFY(  tree2.isSubPath("/home"));

        tree2.insert("/home");
        QVERIFY(! tree2.isSubPath("/", false));
        QVERIFY( tree2.isSubPath("/", true));
        QVERIFY(  tree2.isSubPath("/home"));
        QVERIFY(  tree2.isSubPath("/home/foo"));
    }

    void testFindSub(){        
        PathTree tree;
        tree.insert("/");
        QVERIFY(tree.begin() != tree.end());
        QVERIFY(tree.subpathIter("/") == tree.end());

        tree.insert("/home");
        QVERIFY(tree.subpathIter("/") != tree.end());
        QVERIFY2(*tree.subpathIter("/") == "/home", (*tree.subpathIter("/")).c_str());

        tree.insert("/home/user");
        tree.insert("/var");

        checkAllSubPathsExist(tree, "/", {"/home", "/home/user", "/var"});

        tree.clear();
        QVERIFY(tree.begin() == tree.end());

        tree.insert("/home/foo");
        tree.insert("/media/data/123");
        tree.insert("/media/data/456");
        tree.insert("/media/data/789");

        checkAllSubPathsExist(tree, "/media",
            {"/media/data/123",
             "/media/data/456",
             "/media/data/789",
            });
    }

    void testClear(){
        PathTree tree;
        tree.insert("/home/user");
        tree.clear();
        QVERIFY(! tree.isSubPath("/home/user/foo"));
        tree.insert("/home/user");
        QVERIFY(tree.isSubPath("/home/user/foo"));
    }


    void testCopy(){
        PathTree t1;
        t1.insert("/home/user");
        t1.insert("/home/user/foo");
        t1.insert("/media/cdrom");

        PathTree t2(t1);
        t1.clear();

        checkAllExist(t2, {
                          "/home/user",
                          "/home/user/foo",
                          "/media/cdrom",
                      });

        QVERIFY(t2.isSubPath("/home/user/foo"));
        QVERIFY(t2.isParentPath("/home"));
        QVERIFY(t2.isParentPath("/media"));
        QVERIFY(! t2.isSubPath("/media/aha"));

        t2.erase(t2.iter("/media"));

        PathTree t3 = t2;
        t2.clear();
        checkAllExist(t3, {
                          "/home/user",
                          "/home/user/foo",
                      });
        QVERIFY(t3.isSubPath("/home/user/foo"));
        QVERIFY(t3.isParentPath("/home"));

    }

    void testIter(){
        PathTree tree;
        QVERIFY(tree.begin() == tree.end());

        std::unordered_set<std::string> paths {
            "/home/user/foodir",
            "/home/user/another",
            "/media/cdrom/aha",
            "/media/ok/123",
            "/var/log",
            "/"
        };
        tree.insert(paths.begin(), paths.end());

        checkAllExist(tree, paths);

    }

    void testErase(){
        PathTree tree;
        const std::unordered_set<std::string> paths {
            "/home/user",
            "/home/user/sub1",
            "/home/user/sub2/subsub1",
            "/media/cdrom",
            "/var",
            "/"
        };

        tree.insert(paths.begin(), paths.end());
        auto it = tree.iter("/home");
        QVERIFY(it != tree.end());
        erasePathTreeFromIt(tree, it);
        checkAllExist(tree, {"/media/cdrom", "/var", "/"});


        tree.insert(paths.begin(), paths.end());
        it = tree.iter("/home/user");
        QVERIFY(it != tree.end());
        erasePathTreeFromIt(tree, it);
        checkAllExist(tree, {"/media/cdrom", "/var", "/"});

        tree.insert(paths.begin(), paths.end());
        it = tree.iter("/home/user/sub1");
        QVERIFY(it != tree.end());
        erasePathTreeFromIt(tree, it);
        checkAllExist(tree, {
                          "/home/user",
                          "/home/user/sub2/subsub1",
                          "/media/cdrom",
                          "/var",
                          "/",
                      });

        tree.insert(paths.begin(), paths.end());
        it = tree.iter("/home/user/sub2/subsub1");
        QVERIFY(it != tree.end());
        erasePathTreeFromIt(tree, it);
        checkAllExist(tree, {
                          "/home/user",
                          "/home/user/sub1",
                          "/media/cdrom",
                          "/var",
                          "/",
                      });

        tree.insert(paths.begin(), paths.end());
        it = tree.iter("/var");
        QVERIFY(it != tree.end());
        erasePathTreeFromIt(tree, it);
        checkAllExist(tree, {
                          "/home/user",
                          "/home/user/sub1",
                          "/home/user/sub2/subsub1",
                          "/media/cdrom",
                          "/",
                      });

        tree.insert(paths.begin(), paths.end());
        it = tree.iter("/");
        QVERIFY(it != tree.end());
        erasePathTreeFromIt(tree, it);
        checkAllExist(tree, {});
    }
};


DECLARE_TEST(PathTreeTest)

#include "test_pathtree.moc"
