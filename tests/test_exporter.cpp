#include <QtTest/QtTest>

#include "app/Exporter.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace faraday;

namespace {

QList<SampleRow> makeSamples()
{
    QList<SampleRow> rows;
    for (int i = 0; i < 3; ++i) {
        SampleRow row;
        row.id = i + 1;
        row.tsMs = 1750000000000ll + i * 30000ll;
        row.instanceName = QStringLiteral("ACPI\\PNP0C0A\\1_0");
        row.remainingmWh = 32000 - i * 100;
        row.fullChargedmWh = 42401;
        row.chargeRatemW = 0;
        row.dischargeRatemW = 8000;
        row.voltagemV = 12686;
        row.percent = 75.5 - i;
        row.temperatureC = 33.0;
        row.powerOnline = false;
        row.charging = false;
        row.discharging = true;
        row.cycleCount = 35;
        rows.append(row);
    }
    return rows;
}

ReportInput makeReportInput()
{
    ReportInput input;
    input.appVersion = QStringLiteral("0.1.0");
    input.generatedAt = QDateTime(QDate(2026, 7, 12), QTime(10, 0));
    input.batteryName = QStringLiteral("AE03041");
    input.manufacturer = QStringLiteral("ACME Corp");
    input.chemistry = QStringLiteral("LIon");
    input.serialNumber = QStringLiteral("SN-TEST-0001");
    input.healthPercent = 94.3;
    input.wearPercent = 5.7;
    input.cycleCount = 35;
    input.designmWh = 42401;
    input.fullChargemWh = 40000;
    input.gradeName = QStringLiteral("Excellent");
    input.gradeColor = QStringLiteral("#3fb950");
    input.verdictHeadline = QStringLiteral("Battery in excellent shape <test>");
    input.verdictDetails << QStringLiteral("Detail one") << QStringLiteral("Detail two");
    input.insights << QStringLiteral("Insight one");
    for (int i = 0; i < 4; ++i) {
        CapacityHistoryRow row;
        row.startDate = QDate(2026, 5, 1).addDays(i * 7);
        row.fullChargemWh = 42413 - i * 100;
        row.designmWh = 42413;
        row.cycleCount = 34 + i;
        row.source = QStringLiteral("powercfg");
        input.history.append(row);
    }
    return input;
}

} // namespace

class TestExporter : public QObject
{
    Q_OBJECT

    QString m_dir;

private slots:
    void initTestCase()
    {
        m_dir = QDir::temp().filePath(QStringLiteral("faraday_test_export"));
        QDir(m_dir).removeRecursively();
        QDir().mkpath(m_dir);
    }

    void csvRoundTrip()
    {
        const QString path = QDir(m_dir).filePath(QStringLiteral("samples.csv"));
        QString error;
        QVERIFY2(Exporter::exportCsv(makeSamples(), path, &error), qPrintable(error));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList lines = QString::fromUtf8(file.readAll())
                                      .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 4); // header + 3 rows
        QVERIFY(lines.first().startsWith(QStringLiteral("timestamp_iso,")));
        QCOMPARE(lines.first().split(QLatin1Char(',')).size(), 14);
        QVERIFY(lines.at(1).contains(QStringLiteral("32000")));
        QVERIFY(lines.at(1).contains(QStringLiteral("75.50")));
    }

    void csvEscapesSpecialCharacters()
    {
        QList<SampleRow> rows = makeSamples().mid(0, 1);
        rows[0].instanceName = QStringLiteral("weird,\"name\"");
        const QString path = QDir(m_dir).filePath(QStringLiteral("escaped.csv"));
        QVERIFY(Exporter::exportCsv(rows, path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.contains(QStringLiteral("\"weird,\"\"name\"\"\"")));
    }

    void jsonRoundTrip()
    {
        const QString path = QDir(m_dir).filePath(QStringLiteral("samples.json"));
        QString error;
        QVERIFY2(Exporter::exportJson(makeSamples(), path, &error), qPrintable(error));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object().value(QStringLiteral("sampleCount")).toInt(), 3);
        const QJsonArray samples = doc.object().value(QStringLiteral("samples")).toArray();
        QCOMPARE(samples.size(), 3);
        const QJsonObject first = samples.first().toObject();
        QCOMPARE(first.value(QStringLiteral("remainingmWh")).toInt(), 32000);
        QCOMPARE(first.value(QStringLiteral("discharging")).toBool(), true);
    }

    void htmlReportIsSelfContained()
    {
        const QString html = Exporter::buildHtmlReport(makeReportInput());
        QVERIFY(html.startsWith(QStringLiteral("<!DOCTYPE html>")));
        QVERIFY(html.contains(QStringLiteral("94.3")));
        QVERIFY(html.contains(QStringLiteral("Excellent")));
        QVERIFY(html.contains(QStringLiteral("ACME Corp")));
        QVERIFY(html.contains(QStringLiteral("<svg")));       // inline chart
        QVERIFY(html.contains(QStringLiteral("Detail one")));
        QVERIFY(html.contains(QStringLiteral("Insight one")));
        // Verdict text must be HTML-escaped
        QVERIFY(html.contains(QStringLiteral("&lt;test&gt;")));
        QVERIFY(!html.contains(QStringLiteral("<test>")));
        // Self-contained: no external fetches of any kind
        QVERIFY(!html.contains(QStringLiteral("http://"),  Qt::CaseInsensitive)
                || html.count(QStringLiteral("http://www.w3.org")) ==
                   html.count(QStringLiteral("http://"), Qt::CaseInsensitive));
        QVERIFY(!html.contains(QStringLiteral("https://"), Qt::CaseInsensitive));
        QVERIFY(!html.contains(QStringLiteral("<script"), Qt::CaseInsensitive));
        QVERIFY(!html.contains(QStringLiteral("<link"), Qt::CaseInsensitive));
        QVERIFY(!html.contains(QStringLiteral("src=\"http"), Qt::CaseInsensitive));
    }

    void htmlReportWritesFile()
    {
        const QString path = QDir(m_dir).filePath(QStringLiteral("report.html"));
        QString error;
        QVERIFY2(Exporter::writeHtmlReport(makeReportInput(), path, &error),
                 qPrintable(error));
        QVERIFY(QFile::exists(path));
        QVERIFY(QFileInfo(path).size() > 2000);
    }

    void handlesEmptyInputs()
    {
        const QString path = QDir(m_dir).filePath(QStringLiteral("empty.csv"));
        QVERIFY(Exporter::exportCsv({}, path));
        QVERIFY(Exporter::exportJson({}, QDir(m_dir).filePath(QStringLiteral("empty.json"))));
        ReportInput blank;
        blank.generatedAt = QDateTime::currentDateTime();
        const QString html = Exporter::buildHtmlReport(blank);
        QVERIFY(html.contains(QStringLiteral("Health unknown")));
    }

    void failsCleanlyOnBadPath()
    {
        QString error;
        QVERIFY(!Exporter::exportCsv(makeSamples(),
                                     QStringLiteral("Z:\\no\\such\\dir\\x.csv"), &error));
        QVERIFY(!error.isEmpty());
    }

    void cleanupTestCase()
    {
        QDir(m_dir).removeRecursively();
    }
};

QTEST_APPLESS_MAIN(TestExporter)
#include "test_exporter.moc"
