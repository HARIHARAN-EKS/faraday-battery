#include <QtTest/QtTest>

// Phase 0 gate: the toolchain compiles, links against QtTest and runs.
class TestScaffold : public QObject
{
    Q_OBJECT
private slots:
    void toolchainWorks()
    {
        QVERIFY(sizeof(void *) == 8); // 64-bit build expected
        QCOMPARE(QStringLiteral("faraday").toUpper(), QStringLiteral("FARADAY"));
    }
};

QTEST_APPLESS_MAIN(TestScaffold)
#include "test_scaffold.moc"
