
#pragma once

#include <QTest>
#include <QList>
#include <QString>
#include <QSharedPointer>
#include <QStandardPaths>

#include <memory>

#include "qoutstream.h"
#include "subprocess.h"
#include "qoptargparse/qoptargparse.h"
#include "app.h"
#include "helper_for_test.h"
#include "settings.h"
#include "logger.h"


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
    if(! shournal_common_init()){
        QIErr() << qtr("Fatal error: failed to initialize custom Qt conversion functions");
        exit(1);
    }
    logger::setup("shournal-test");
    logger::setVerbosityLevel(QtMsgType::QtWarningMsg);

    // ignore first arg (command to this app)
    --argc;
    ++argv;

    QOptArgParse parser;
    QOptArg argVerbosity("", "verbosity", qtr("How much shall be printed to stderr. Note that "
                                              "for 'dbg' shournal must not be a 'Release'-build, "
                                              "dbg-messages are lost in Release-mode."));
    argVerbosity.setAllowedOptions({"dbg",
                                #if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
                                    "info",
                                #endif
                                    "warning", "critical"});
    parser.addArg(&argVerbosity);

    QOptArg argIntegrationTest("", "integration",
                               "Run integration tests, instead of normal tests", false);
    parser.addArg(&argIntegrationTest);


    QOptArg argShell("", "shell", "The shell used for the intgeration tests, including"
                                  " arguments, separated by whitespace");
    argShell.addRequiredArg(&argIntegrationTest);
    parser.addArg(&argShell);

    parser.parse(argc, argv);

    if(argVerbosity.wasParsed()){
        QByteArray verbosity = argVerbosity.getOptions(1).first().toLocal8Bit();
        logger::setVerbosityLevel(verbosity.constData());
    }

    if(argIntegrationTest.wasParsed()){
        os::setenv<QByteArray>("_SHOURNAL_IN_INTEGRATION_TEST_MODE", "true");
        app::setupNameAndVersion("shournal-integration-test");
        if(! app::inIntegrationTestMode()){
            QIErr() << "Failed to enable integration test mode.";
            exit(1);
        }

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
        if(argIntegrationTest.wasParsed()){
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

