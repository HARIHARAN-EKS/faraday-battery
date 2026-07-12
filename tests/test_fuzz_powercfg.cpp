#include <QtTest/QtTest>

#include "PowercfgFixture.h"
#include "PropertyGen.h"
#include "acquisition/PowercfgReport.h"

#include <QElapsedTimer>

using namespace faraday;
using namespace faraday::propgen;

// Fuzzing of the powercfg XML parser: it must never crash, never hang and
// never fabricate data, no matter what bytes arrive.
class TestFuzzPowercfg : public QObject
{
    Q_OBJECT

    static QByteArray fixture() { return QByteArray(kSampleBatteryReportXml); }

private slots:
    void structuralCorpus_data()
    {
        QTest::addColumn<QByteArray>("xml");
        QTest::addColumn<bool>("mayParse"); // false => parser MUST report failure

        QTest::newRow("empty") << QByteArray() << false;
        QTest::newRow("whitespace") << QByteArray("   \n\t  ") << false;
        QTest::newRow("single-angle") << QByteArray("<") << false;
        QTest::newRow("decl-only") << QByteArray("<?xml version=\"1.0\"?>") << false;
        QTest::newRow("wrong-root")
            << QByteArray("<?xml version=\"1.0\"?><NotABatteryReport/>") << false;
        QTest::newRow("html") << QByteArray("<html><body>404</body></html>") << false;
        QTest::newRow("json") << QByteArray("{\"batteries\": []}") << false;
        QTest::newRow("null-bytes") << QByteArray("\0\0\0\0", 4) << false;
        QTest::newRow("binary-garbage")
            << QByteArray("\x89PNG\r\n\x1a\n\xde\xad\xbe\xef", 12) << false;
        QTest::newRow("unclosed-root")
            << QByteArray("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">")
            << false;
        QTest::newRow("mismatched-tags")
            << QByteArray("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
                          "<Batteries><Battery></Batteries></Battery></BatteryReport>")
            << false;
        // Valid root, empty content: a desktop-style report parses OK.
        QTest::newRow("empty-report")
            << QByteArray("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
                          "<Batteries/></BatteryReport>")
            << true;
        // Duplicate battery elements: parse, no crash, both or first taken.
        QTest::newRow("duplicate-batteries")
            << QByteArray("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
                          "<Batteries><Battery><Id>A</Id><DesignCapacity>1000</DesignCapacity>"
                          "</Battery><Battery><Id>A</Id><DesignCapacity>2000</DesignCapacity>"
                          "</Battery></Batteries></BatteryReport>")
            << true;
        // Absurd numeric values: overflow must yield absent, not garbage.
        QTest::newRow("overflow-capacity")
            << QByteArray("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
                          "<Batteries><Battery><DesignCapacity>99999999999999999999999"
                          "</DesignCapacity></Battery></Batteries></BatteryReport>")
            << true;
        QTest::newRow("negative-capacity")
            << QByteArray("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
                          "<Batteries><Battery><DesignCapacity>-42</DesignCapacity>"
                          "</Battery></Batteries></BatteryReport>")
            << true;
        QTest::newRow("non-numeric-capacity")
            << QByteArray("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
                          "<Batteries><Battery><DesignCapacity>lots</DesignCapacity>"
                          "</Battery></Batteries></BatteryReport>")
            << true;
        // Non-UTF8 bytes inside a text node.
        QByteArray latin("<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
                         "<Batteries><Battery><Manufacturer>");
        latin.append('\xC0');
        latin.append('\xC1');
        latin.append('\xFE');
        latin.append("</Manufacturer></Battery></Batteries></BatteryReport>");
        QTest::newRow("invalid-utf8") << latin << true;
    }
    void structuralCorpus()
    {
        QFETCH(QByteArray, xml);
        QFETCH(bool, mayParse);
        const PowercfgReportData data = PowercfgReport::parse(xml);
        if (!mayParse)
            QVERIFY(!data.ok); // malformed input must be reported as failure
        // Universal invariants — regardless of ok:
        QVERIFY(data.batteries.size() <= 64);
        for (const auto &b : data.batteries) {
            if (b.designCapacitymWh.has_value())
                QVERIFY(*b.designCapacitymWh > -1000000000000ll); // parsed, not wild
        }
    }

    void xxeEntitiesAreInert()
    {
        // External-entity attempt pointing at a real local file. The parser
        // must not resolve it: no file content may surface anywhere.
        const QByteArray xxe =
            "<?xml version=\"1.0\"?>"
            "<!DOCTYPE BatteryReport [<!ENTITY xxe SYSTEM \"file:///C:/Windows/win.ini\">"
            "<!ENTITY internal \"INTERNALVALUE\">]>"
            "<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
            "<Batteries><Battery><Manufacturer>&xxe;</Manufacturer>"
            "<SerialNumber>&internal;</SerialNumber></Battery></Batteries></BatteryReport>";
        const PowercfgReportData data = PowercfgReport::parse(xxe);
        for (const auto &b : data.batteries) {
            // win.ini starts with "; for 16-bit app support" / "[fonts]".
            QVERIFY(!b.manufacturer.contains(QStringLiteral("fonts"), Qt::CaseInsensitive));
            QVERIFY(!b.manufacturer.contains(QStringLiteral("16-bit")));
            QVERIFY(b.manufacturer.length() < 4096);
        }
        // A billion-laughs style expansion must not hang or explode memory.
        const QByteArray bomb =
            "<?xml version=\"1.0\"?>"
            "<!DOCTYPE r [<!ENTITY a \"aaaaaaaaaa\"><!ENTITY b \"&a;&a;&a;&a;&a;&a;&a;&a;\">"
            "<!ENTITY c \"&b;&b;&b;&b;&b;&b;&b;&b;\"><!ENTITY d \"&c;&c;&c;&c;&c;&c;&c;&c;\">]>"
            "<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
            "<Batteries><Battery><Id>&d;&d;&d;&d;</Id></Battery></Batteries></BatteryReport>";
        QElapsedTimer t;
        t.start();
        const PowercfgReportData bombData = PowercfgReport::parse(bomb);
        QVERIFY(t.elapsed() < 5000); // must not hang
        for (const auto &b : bombData.batteries)
            QVERIFY(b.id.size() < 1000000); // no unbounded expansion
    }

    void truncationSweep_data()
    {
        QTest::addColumn<int>("keepBytes");
        const int len = fixture().size();
        for (int i = 0; i < 40; ++i)
            QTest::addRow("keep=%d", (len * i) / 40) << (len * i) / 40;
    }
    void truncationSweep()
    {
        QFETCH(int, keepBytes);
        const PowercfgReportData data = PowercfgReport::parse(fixture().left(keepBytes));
        // Truncation anywhere: no crash; either clean failure or a sane
        // partial parse (bounded sizes, no wild values).
        QVERIFY(data.batteries.size() <= 16);
        QVERIFY(data.history.size() <= 10000);
        QVERIFY(data.usage.size() <= 10000);
    }

    void randomByteMutations_data()
    {
        QTest::addColumn<int>("seed");
        for (int i = 0; i < 300; ++i)
            QTest::addRow("mut%03d", i) << i;
    }
    void randomByteMutations()
    {
        QFETCH(int, seed);
        Gen g(0xF022CF6Bu + seed);
        QByteArray xml = fixture();
        const int mutations = 1 + int(g.u32Below(16));
        for (int m = 0; m < mutations; ++m) {
            const int pos = int(g.u32Below(quint32(xml.size())));
            switch (g.u32Below(3)) {
            case 0: xml[pos] = char(g.u32Below(256)); break;              // flip
            case 1: xml.insert(pos, char(g.u32Below(256))); break;        // insert
            default: xml.remove(pos, 1 + int(g.u32Below(8))); break;      // delete
            }
        }
        QElapsedTimer t;
        t.start();
        const PowercfgReportData data = PowercfgReport::parse(xml);
        QVERIFY2(t.elapsed() < 5000, "parser hung on mutated input");
        // Sanity bounds — a parser gone wrong would blow these up.
        QVERIFY(data.batteries.size() <= 16);
        QVERIFY(data.history.size() <= 10000);
        QVERIFY(data.usage.size() <= 10000);
        for (const auto &u : data.usage)
            QVERIFY(u.duration100ns >= -8640000000000000ll
                    && u.duration100ns <= 8640000000000000ll * 100);
    }

    void hugeSyntheticReportParsesBounded()
    {
        // ~20k weekly history entries + 20k usage entries (~5 MB of XML).
        QByteArray xml;
        xml.reserve(6 * 1024 * 1024);
        xml += "<BatteryReport xmlns=\"http://schemas.microsoft.com/battery/2012\">"
               "<Batteries><Battery><Id>BIG</Id><DesignCapacity>50000</DesignCapacity>"
               "<FullChargeCapacity>48000</FullChargeCapacity></Battery></Batteries><History>";
        for (int i = 0; i < 20000; ++i) {
            xml += QByteArray("<HistoryEntry StartDate=\"2020-01-01T00:00:00\" "
                              "EndDate=\"2020-01-08T00:00:00\" DesignCapacity=\"50000\" "
                              "FullChargeCapacity=\"")
                   + QByteArray::number(48000 - (i % 5000)) + "\" CycleCount=\""
                   + QByteArray::number(i % 900) + "\"/>";
        }
        xml += "</History><RecentUsage>";
        for (int i = 0; i < 20000; ++i) {
            xml += QByteArray("<UsageEntry Timestamp=\"2020-01-01T00:00:00\" "
                              "LocalTimestamp=\"2020-01-01T00:00:00\" EntryType=\"Active\" "
                              "Ac=\"0\" ChargeCapacity=\"30000\" Discharge=\"500\" "
                              "FullChargeCapacity=\"48000\" Duration=\"36000000000\"/>");
        }
        xml += "</RecentUsage></BatteryReport>";

        QElapsedTimer t;
        t.start();
        const PowercfgReportData data = PowercfgReport::parse(xml);
        const qint64 ms = t.elapsed();
        QVERIFY(data.ok);
        QCOMPARE(data.history.size(), 20000);
        QCOMPARE(data.usage.size(), 20000);
        QVERIFY2(ms < 30000, qPrintable(QStringLiteral("huge parse took %1 ms").arg(ms)));
    }

    void realMachineReportStillParses()
    {
        // Ground-truth integration: the real powercfg on this machine.
        const PowercfgReportData data =
            PowercfgReport::generate(QDir::tempPath());
        if (!data.ok)
            QSKIP("powercfg unavailable in this environment");
        QVERIFY(!data.batteries.isEmpty());
        QVERIFY(data.batteries.first().designCapacitymWh.value_or(0) > 0);
    }
};

QTEST_GUILESS_MAIN(TestFuzzPowercfg)
#include "test_fuzz_powercfg.moc"
