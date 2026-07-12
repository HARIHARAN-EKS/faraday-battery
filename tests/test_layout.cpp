#include <QtTest/QtTest>

#include "app/BatteryModel.h"
#include "app/TrayManager.h"
#include "SyntheticData.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>

using namespace faraday;

// Layout regression tests for the QML pages (defect C1).
//
// The bug class: an element anchored as an OVERLAY on top of a sibling that
// owns the same coordinate space (a chart heading or legend drawn over a
// Canvas). Nothing in the scene graph prevents them from colliding, so the
// collision appears only at certain sizes or data ranges.
//
// The fix is a layout CONSTRAINT: the heading/legend and the plot are rows of
// a ColumnLayout, so the plot can never reach the heading. These tests assert
// that constraint directly, on the real pages, at several window sizes and in
// both themes. They fail against the pre-fix QML.
//
// Rendering runs on the offscreen platform, set in main() before the
// QGuiApplication exists.
class TestLayout : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dataDir;
    BatteryModel m_model;
    TrayManager m_tray;

    // Loads one page inside a window of the given size and returns the window.
    // The caller owns it.
    QQuickWindow *loadPage(QQmlApplicationEngine &engine, const QString &page,
                           int w, int h, QString *error)
    {
        const QString qml = QStringLiteral(
            "import QtQuick\n"
            "import QtQuick.Window\n"
            "import Faraday\n"
            "Window {\n"
            "    width: %1; height: %2; visible: true\n"
            "    %3 { objectName: \"page\"; anchors.fill: parent }\n"
            "}\n").arg(w).arg(h).arg(page);

        QQmlComponent component(&engine);
        component.setData(qml.toUtf8(), QUrl(QStringLiteral("qrc:/layouttest.qml")));
        if (component.isError()) {
            *error = component.errorString();
            return nullptr;
        }
        QObject *obj = component.create();
        auto *window = qobject_cast<QQuickWindow *>(obj);
        if (!window) {
            *error = QStringLiteral("root is not a Window");
            delete obj;
            return nullptr;
        }
        // Let bindings settle and the layouts run.
        QTest::qWait(150);
        return window;
    }

    static QQuickItem *find(QQuickWindow *window, const QString &objectName)
    {
        return window->findChild<QQuickItem *>(objectName);
    }

    // Bounding box of an item in window coordinates.
    static QRectF boxOf(QQuickItem *item)
    {
        const QPointF topLeft = item->mapToScene(QPointF(0, 0));
        return QRectF(topLeft, QSizeF(item->width(), item->height()));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dataDir.isValid());
        m_model.initialize(m_dataDir.path());
        // Real-looking data: a worn pack with capacity history, so the charts
        // actually paint axis labels (the empty-chart path draws none).
        m_model.applySnapshot(synthetic::snapshot({ 33.0 }, 65.0));
        m_model.applyReport(synthetic::decliningReport(20, 52007, 40000, 120));
    }

    void chartHeadingsNeverOverlapTheirPlots_data()
    {
        QTest::addColumn<QString>("page");
        QTest::addColumn<QString>("headingName");
        QTest::addColumn<QString>("plotName");
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("height");
        QTest::addColumn<bool>("dark");

        struct Target { const char *page; const char *heading; const char *plot; };
        const Target targets[] = {
            // The reported defect: "CAPACITY DECLINE" over the Y-axis labels.
            { "HistoryPage", "capacityHeading", "capacityChart" },
            { "HistoryPage", "cycleHeading", "cycleChart" },
            // The same bug class, found in the audit: the legend sat on top of
            // the plot area, where a 100 %-charge line would run under it.
            { "MonitorPage", "chartLegend", "liveChart" },
        };
        struct Size { const char *name; int w; int h; };
        const Size sizes[] = {
            { "minimum", 760, 560 },     // the window's minimum size
            { "awkward", 1024, 620 },    // an intermediate width
            { "maximised", 1920, 1080 },
        };

        for (const Target &t : targets) {
            for (const Size &s : sizes) {
                for (const bool dark : { true, false }) {
                    QTest::addRow("%s/%s/%s/%s", t.page, t.heading, s.name,
                                  dark ? "dark" : "light")
                        << QString::fromLatin1(t.page)
                        << QString::fromLatin1(t.heading)
                        << QString::fromLatin1(t.plot)
                        << s.w << s.h << dark;
                }
            }
        }
    }
    void chartHeadingsNeverOverlapTheirPlots()
    {
        QFETCH(QString, page);
        QFETCH(QString, headingName);
        QFETCH(QString, plotName);
        QFETCH(int, width);
        QFETCH(int, height);
        QFETCH(bool, dark);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("battery"), &m_model);
        engine.rootContext()->setContextProperty(QStringLiteral("tray"), &m_tray);

        QString error;
        QQuickWindow *window = loadPage(engine, page, width, height, &error);
        QVERIFY2(window, qPrintable(error));

        // Theme is a QML singleton; flip it through the model's setting, which
        // Main.qml normally mirrors into Theme.dark. Here we set it directly on
        // the singleton instance so both themes are genuinely exercised.
        QObject *theme = engine.singletonInstance<QObject *>(
            QStringLiteral("Faraday"), QStringLiteral("Theme"));
        QVERIFY(theme);
        theme->setProperty("dark", dark);
        QTest::qWait(100);

        QQuickItem *heading = find(window, headingName);
        QQuickItem *plot = find(window, plotName);
        QVERIFY2(heading, qPrintable(QStringLiteral("no item named %1").arg(headingName)));
        QVERIFY2(plot, qPrintable(QStringLiteral("no item named %1").arg(plotName)));

        const QRectF headingBox = boxOf(heading);
        const QRectF plotBox = boxOf(plot);

        QVERIFY2(headingBox.height() > 0 && plotBox.height() > 0,
                 "both items must have been laid out");

        // THE CONSTRAINT: the plot area starts strictly below the heading, so
        // no axis label the canvas paints at its top edge can reach it.
        QVERIFY2(plotBox.top() >= headingBox.bottom(),
                 qPrintable(QStringLiteral("%1: heading bottom %2 overlaps plot top %3 "
                                           "(%4x%5, %6)")
                                .arg(page).arg(headingBox.bottom()).arg(plotBox.top())
                                .arg(width).arg(height)
                                .arg(dark ? QStringLiteral("dark") : QStringLiteral("light"))));
        QVERIFY2(!headingBox.intersects(plotBox),
                 qPrintable(QStringLiteral("%1: heading box intersects plot box")
                                .arg(page)));

        delete window;
    }

    void everyPageLoadsWithoutErrorAtEverySize_data()
    {
        QTest::addColumn<QString>("page");
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("height");

        const char *pages[] = { "DashboardPage", "MonitorPage", "HistoryPage",
                                "AlertsPage", "CalibrationPage", "SettingsPage" };
        struct Size { const char *name; int w; int h; };
        const Size sizes[] = {
            { "minimum", 760, 560 },
            { "awkward", 1024, 620 },
            { "maximised", 1920, 1080 },
        };
        for (const char *p : pages) {
            for (const Size &s : sizes)
                QTest::addRow("%s/%s", p, s.name) << QString::fromLatin1(p) << s.w << s.h;
        }
    }
    void everyPageLoadsWithoutErrorAtEverySize()
    {
        QFETCH(QString, page);
        QFETCH(int, width);
        QFETCH(int, height);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("battery"), &m_model);
        engine.rootContext()->setContextProperty(QStringLiteral("tray"), &m_tray);

        QString error;
        QQuickWindow *window = loadPage(engine, page, width, height, &error);
        QVERIFY2(window, qPrintable(QStringLiteral("%1 @ %2x%3: %4")
                                        .arg(page).arg(width).arg(height).arg(error)));

        QQuickItem *pageItem = find(window, QStringLiteral("page"));
        QVERIFY(pageItem);
        // The page filled its window: nothing collapsed to zero.
        QVERIFY(pageItem->width() > 0 && pageItem->height() > 0);

        delete window;
    }
};

int main(int argc, char *argv[])
{
    // Must be set before the application is constructed.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestLayout tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "test_layout.moc"
