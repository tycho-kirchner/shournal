


#include <QTest>
#include <QDebug>
#include <QTemporaryFile>

#include "autotest.h"

#include "qoptargparse/qoptargparse.h"
#include "qoptargparse/qoptsqlarg.h"
#include "qoptargparse/qoptvarlenarg.h"


class QOptArgparseTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){
        logger::setup(__FILE__);
    }

    void testIt() {
        QOptArgParse parser;
        QOptArg arg1("", "one", "");
        parser.addArg(&arg1);

        QOptSqlArg arg2Sql("", "two", "", {E_CompareOperator::EQ} );
        parser.addArg(&arg2Sql);

        QOptSqlArg arg3Sql("", "three", "", {E_CompareOperator::EQ} );
        parser.addArg(&arg3Sql);

        QOptSqlArg arg4Sql("", "four", "", {E_CompareOperator::BETWEEN}, E_CompareOperator::BETWEEN );
        parser.addArg(&arg4Sql);

        QOptVarLenArg arg5VarLen("", "five", "");
        parser.addArg(&arg5VarLen);

        QOptArg arg6("", "six", "", false);
        arg6.setFinalizeFlag(true);
        parser.addArg(&arg6);

        QVector<const char*> argv = {"--one", "1",
                              "--two", "-eq", "2",
                              "--three", "3",
                              "--four", "-between", "4_1", "4_2",
                              "--five", "3", "5_1", "5_2", "5_3",
                              "--six", "6_1", "6_2", "6_3", "6_4",
                              nullptr};

        parser.parse(argv.size() - 1, (char**)argv.data());

        QVERIFY(arg1.wasParsed());
        QCOMPARE(arg1.getValue<QString>(), QString("1"));

        QVERIFY(arg2Sql.wasParsed());
        QCOMPARE(arg2Sql.getValue<QString>(), QString("2"));
        QCOMPARE(arg2Sql.parsedOperator(), E_CompareOperator::EQ);

        QVERIFY(arg3Sql.wasParsed());
        QCOMPARE(arg3Sql.getValue<QString>(), QString("3"));
        QCOMPARE(arg3Sql.parsedOperator(), E_CompareOperator::EQ);

        QVERIFY(arg4Sql.wasParsed());
        QCOMPARE(arg4Sql.getValues<QStringList>(), QStringList({"4_1", "4_2"}));
        QCOMPARE(arg4Sql.parsedOperator(), E_CompareOperator::BETWEEN);

        QVERIFY(arg5VarLen.wasParsed());
        QCOMPARE(arg5VarLen.getValues<QStringList>(), QStringList({"5_1", "5_2", "5_3"}));

        QVERIFY(arg6.wasParsed());
        QCOMPARE(parser.rest().len, 4);
        const char* arg6Actual[] = {"6_1", "6_2", "6_3", "6_4"};
        for(int i=0; i < parser.rest().len; i++ ){
            QVERIFY(strcmp(parser.rest().argv[i], arg6Actual[i]) == 0);
        }
    }


};


DECLARE_TEST(QOptArgparseTest)

#include "test_qoptargparse.moc"
