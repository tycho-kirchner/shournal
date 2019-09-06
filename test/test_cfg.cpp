
#include <memory>
#include <QTest>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDebug>

#include "cfg.h"

#include "autotest.h"

using qsimplecfg::Cfg;


class CfgTest : public QObject {
    Q_OBJECT

    const QString CONFIG_TXT = R"SOMERANDOMTEXT(
# Initial
# comment

[section1]
# section1
# comment
section1_key1=val1
section1_key2 =  val2

[section2]
# section2
# comment
section2_key1 = '''first
    second
    third'''
section2_key2='''
one
    two

three
'''

[section3]
section3_key1 = '''
'''
section3_key2 = '''
foo
'''

)SOMERANDOMTEXT";

    void verifyStdCfg(Cfg& cfg){

        QVERIFY(cfg.m_parsedNameSectionHash.find("section1") != cfg.m_parsedNameSectionHash.end());
        auto sect1 = cfg["section1"];
        sect1->setComments("section1\ncomment");
        QCOMPARE(sect1->getValue<QString>("section1_key1"), QString("val1") );
        QCOMPARE(sect1->getValue<QString>("section1_key2"), QString("val2") );

        QVERIFY(cfg.m_parsedNameSectionHash.find("section2") != cfg.m_parsedNameSectionHash.end());
        auto  sect2 = cfg["section2"];
        QCOMPARE(sect2->getValues<QStringList>("section2_key1", {},false, "\n"),
                                            (QStringList{"first", "second", "third"}) );
        QCOMPARE(sect2->getValues<QStringList>("section2_key2", {},false, "\n"),
                 (QStringList{"one", "two", "three"}) );

        QVERIFY(cfg.m_parsedNameSectionHash.find("section3") != cfg.m_parsedNameSectionHash.end());
        auto  sect3 = cfg["section3"];
        QCOMPARE(sect3->getValues<QStringList>("section3_key1", {}, false, "\n"), QStringList());
        QCOMPARE(sect3->getValues<QStringList>("section3_key2", {}, false, "\n"),
                 (QStringList{"foo"}) );

        QVERIFY(cfg.generateNonReadSectionKeyPairs().isEmpty());
    }

    std::unique_ptr<QTemporaryFile> writeToTmpConfigFile(const QString& txt){
        auto file = std::unique_ptr<QTemporaryFile>(new QTemporaryFile);
        if(! file->open()){
            return file;
        }
        QTextStream stream(file.get());
        stream << txt;
        file->close();
        return file;
    }

private slots:
    void tgeneral() {
        auto file = writeToTmpConfigFile(CONFIG_TXT);
        QVERIFY(! file->fileName().isEmpty());

        Cfg cfg;
        // parse, verify, store and verify again
        cfg.parse(file->fileName());
        verifyStdCfg(cfg);
        cfg.store(file->fileName());

        cfg.parse(file->fileName());
        verifyStdCfg(cfg);
    }

    void tRenameSection(){
        auto file = writeToTmpConfigFile(CONFIG_TXT);
        QVERIFY(! file->fileName().isEmpty());

        Cfg cfg;
        cfg.parse(file->fileName());
        QVERIFY(cfg.renameParsedSection("section1", "section1_renamed"));

        auto sect1 = cfg["section1_renamed"];
        QCOMPARE(sect1->getValue<QString>("section1_key1"), QString("val1") );
        QCOMPARE(sect1->getValue<QString>("section1_key2"), QString("val2") );

        cfg.store(file->fileName());

        // now that it's renamed, skip the renaming and check if values still correct
        cfg.parse(file->fileName());

        sect1 = cfg["section1_renamed"];
        QCOMPARE(sect1->getValue<QString>("section1_key1"), QString("val1") );
        QCOMPARE(sect1->getValue<QString>("section1_key2"), QString("val2") );
    }

    void tRenameKey(){
        auto file = writeToTmpConfigFile(CONFIG_TXT);
        QVERIFY(! file->fileName().isEmpty());

        Cfg cfg;
        cfg.parse(file->fileName());
        auto sect = cfg.getParsedSectionIfExist("section1");
        QVERIFY(sect != nullptr);
        QVERIFY(sect->renameParsedKey("section1_key1", "section1_key1_renamed"));

        // assign it again -> only sections accessed via opertor[] are stored
        // to disk later...
        sect = cfg["section1"];
        QCOMPARE(sect->getValue<QString>("section1_key1_renamed"), QString("val1") );
        QCOMPARE(sect->getValue<QString>("section1_key2"), QString("val2") );

        cfg.store(file->fileName());

        cfg.parse(file->fileName());
        sect = cfg["section1"];
        QCOMPARE(sect->getValue<QString>("section1_key1_renamed"), QString("val1") );
        QCOMPARE(sect->getValue<QString>("section1_key2"), QString("val2") );

        // now that it's renamed, skip the renaming and check if values still correct
    }

};

DECLARE_TEST(CfgTest)

#include "test_cfg.moc"
