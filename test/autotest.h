
#pragma once

#include <QTest>
#include <QList>
#include <QString>
#include <QSharedPointer>
#include <QStandardPaths>

#include "qoutstream.h"
#include "subprocess.h"
#include "qoptargparse/qoptargparse.h"
#include "app.h"
#include "helper_for_test.h"

class ShournalTestGlobals {
public:
    subprocess::Args_t integrationShellArgs;
    std::string integrationSetupCommand;
};



namespace AutoTest
{

inline ShournalTestGlobals& globals(){
    static ShournalTestGlobals globals;
    return globals;
}


typedef QList<QObject*> TestList;

inline TestList& testList()
{
    static TestList list;
    return list;
}

inline bool findObject(QObject* object)
{
    TestList& list = testList();
    if (list.contains(object))
    {
        return true;
    }
    foreach (QObject* test, list)
    {
        if (test->objectName() == object->objectName())
        {
            return true;
        }
    }
    return false;
}

inline void addTest(QObject* object)
{
    TestList& list = testList();
    if (!findObject(object))
    {
        list.append(object);
    }
}

inline int run(int argc, char *argv[])
{
    bool integrationMode = false;
    if(argc > 1) {
        if(strcmp(argv[1], "--integration") == 0){
            integrationMode = true;
        } else {
            QIErr() << "first arg must be '--integration' or nothing.";
            exit(1);
        }
    }
    if(integrationMode){
        os::setenv<QByteArray>("_SHOURNAL_IN_INTEGRATION_TEST_MODE", "true");
        app::setupNameAndVersion();
        if(! app::inIntegrationTestMode()){
            QIErr() << "Failed to enable integration test mode.";
            exit(1);
        }

        QOptArgParse parser;
        QOptArg argShell("", "shell", "the shell and arguments, separated by whitespace");
        parser.addArg(&argShell);
        parser.parse(argc - 2, argv + 2);

        if(! argShell.wasParsed()){
            QIErr() << "missing argument" << argShell.name();
            exit(1);
        }
        const auto shellArgs = argShell.getValue<QString>().split(" ", QString::SkipEmptyParts);
        if(shellArgs.first() == "bash"){
            globals().integrationSetupCommand = "export HISTFILE=/dev/null";
        } else {
            QIErr() << "currently only bash is supported.";
            exit(1);
        }

        for(const QString& s : shellArgs){
            globals().integrationShellArgs.push_back(s.toStdString());
        }
    } else {
        QCoreApplication::setApplicationName(QString(app::SHOURNAL) + "-test");
        QCoreApplication::setApplicationVersion( app::version().toString());
    }

    QStandardPaths::setTestModeEnabled(true);
    // delete remaining paths from last test (if any)
    testhelper::deletePaths();

    int ret = 0;

    foreach (QObject* test, testList())
    {
        if(integrationMode){
            if(! test->objectName().startsWith("IntegrationTest")){
                continue;
            }
        } else{
            if(test->objectName().startsWith("IntegrationTest")){
                continue;
            }

        }
        ret += QTest::qExec(test, {});
    }
    if(ret != 0){
        QErr() << "\n**** AT LEAST ONE TEST FAILED! ****\n\n";
    }

    return ret;
}
}

template <class T>
class Test
{
public:
    QSharedPointer<T> child;

    Test(const QString& name) : child(new T)
    {
        child->setObjectName(name);
        AutoTest::addTest(child.data());
    }
};

#define DECLARE_TEST(className) static Test<className> t(#className);

#define TEST_MAIN \
    int main(int argc, char *argv[]) \
{ \
    return AutoTest::run(argc, argv); \
    }

