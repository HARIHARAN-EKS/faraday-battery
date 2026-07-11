#include <QtTest/QtTest>

#include "core/HealthVerdict.h"

using namespace faraday;

namespace {

PowercfgReportData::UsageEntry usageEntry(bool ac, qint64 chargemWh, qint64 durationSec,
                                          qint64 fccmWh = 40000)
{
    PowercfgReportData::UsageEntry e;
    e.timestamp = QDateTime(QDate(2026, 7, 1), QTime(12, 0));
    e.entryType = QStringLiteral("Active");
    e.ac = ac;
    e.chargeCapacitymWh = chargemWh;
    e.fullChargeCapacitymWh = fccmWh;
    e.duration100ns = durationSec * 10000000ll;
    return e;
}

} // namespace

class TestVerdict : public QObject
{
    Q_OBJECT
private slots:
    void gradeBoundaries()
    {
        QCOMPARE(HealthVerdict::gradeForHealth(100.0), HealthGrade::Excellent);
        QCOMPARE(HealthVerdict::gradeForHealth(90.0), HealthGrade::Excellent);
        QCOMPARE(HealthVerdict::gradeForHealth(89.99), HealthGrade::Good);
        QCOMPARE(HealthVerdict::gradeForHealth(80.0), HealthGrade::Good);
        QCOMPARE(HealthVerdict::gradeForHealth(79.99), HealthGrade::Fair);
        QCOMPARE(HealthVerdict::gradeForHealth(65.0), HealthGrade::Fair);
        QCOMPARE(HealthVerdict::gradeForHealth(64.99), HealthGrade::Worn);
        QCOMPARE(HealthVerdict::gradeForHealth(50.0), HealthGrade::Worn);
        QCOMPARE(HealthVerdict::gradeForHealth(49.99), HealthGrade::ReplaceSoon);
        QCOMPARE(HealthVerdict::gradeForHealth(std::nullopt), HealthGrade::Unknown);
    }

    void gradeNamesNonEmpty()
    {
        for (HealthGrade g : { HealthGrade::Unknown, HealthGrade::Excellent, HealthGrade::Good,
                               HealthGrade::Fair, HealthGrade::Worn, HealthGrade::ReplaceSoon }) {
            QVERIFY(!HealthVerdict::gradeName(g).isEmpty());
        }
    }

    void verdictAlwaysHasHeadline()
    {
        QVERIFY(!HealthVerdict::generate({}).headline.isEmpty());

        VerdictInput input;
        input.healthPercent = 94.3;
        const Verdict v = HealthVerdict::generate(input);
        QCOMPARE(v.grade, HealthGrade::Excellent);
        QVERIFY(!v.headline.isEmpty());
        QVERIFY(!v.details.isEmpty()); // capacity detail present
    }

    void verdictMentionsCycles()
    {
        VerdictInput input;
        input.healthPercent = 85.0;
        input.cycleCount = 35;
        Verdict v = HealthVerdict::generate(input);
        bool found = false;
        for (const QString &d : v.details)
            found = found || d.contains(QStringLiteral("35"));
        QVERIFY(found);

        input.cycleCount = 750; // high-cycle branch
        v = HealthVerdict::generate(input);
        found = false;
        for (const QString &d : v.details)
            found = found || d.contains(QStringLiteral("750"));
        QVERIFY(found);
    }

    void verdictFlagsHeatAndDrift()
    {
        VerdictInput input;
        input.healthPercent = 85.0;
        input.temperatureC = 55.0;
        input.calibrationDriftPercent = -8.2;
        const Verdict v = HealthVerdict::generate(input);
        bool heat = false, drift = false;
        for (const QString &d : v.details) {
            heat = heat || d.contains(QStringLiteral("55"));
            drift = drift || d.contains(QStringLiteral("8.2"));
        }
        QVERIFY(heat);
        QVERIFY(drift);

        // Mild values must NOT trigger the warnings.
        VerdictInput mild;
        mild.healthPercent = 85.0;
        mild.temperatureC = 30.0;
        mild.calibrationDriftPercent = 1.0;
        const Verdict vm = HealthVerdict::generate(mild);
        for (const QString &d : vm.details) {
            QVERIFY(!d.contains(QStringLiteral("calibration cycle")));
            QVERIFY(!d.contains(QStringLiteral("running warm")));
        }
    }

    void insightsEmptyWithoutData()
    {
        QVERIFY(HealthVerdict::chargingHabitInsights({}).isEmpty());
    }

    void insightsDetectFullChargeDwell()
    {
        QList<PowercfgReportData::UsageEntry> usage;
        // 10 h plugged in at 99 %, 2 h on battery at 50 %
        usage << usageEntry(true, 39600, 36000)
              << usageEntry(false, 20000, 7200);
        const QStringList insights = HealthVerdict::chargingHabitInsights(usage);
        QVERIFY(insights.size() >= 2);
        bool capHint = false;
        for (const QString &s : insights)
            capHint = capHint || s.contains(QStringLiteral("charge cap"));
        QVERIFY(capHint);
    }

    void insightsDetectDeepDischarge()
    {
        QList<PowercfgReportData::UsageEntry> usage;
        usage << usageEntry(false, 4000, 3600)   // 10 %
              << usageEntry(false, 5200, 3600)   // 13 %
              << usageEntry(false, 30000, 3600)  // 75 %
              << usageEntry(true, 39000, 3600);
        const QStringList insights = HealthVerdict::chargingHabitInsights(usage);
        bool deep = false;
        for (const QString &s : insights)
            deep = deep || s.contains(QStringLiteral("Deep discharges"));
        QVERIFY(deep);
    }
};

QTEST_APPLESS_MAIN(TestVerdict)
#include "test_verdict.moc"
