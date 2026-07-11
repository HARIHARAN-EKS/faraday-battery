#include "Exporter.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace faraday {

namespace {

QString csvField(const QVariant &v)
{
    if (!v.isValid() || v.isNull())
        return QString();
    QString s = v.toString();
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"'))
        || s.contains(QLatin1Char('\n'))) {
        s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        s = QLatin1Char('"') + s + QLatin1Char('"');
    }
    return s;
}

template <typename T>
QVariant optVar(const std::optional<T> &v)
{
    return v.has_value() ? QVariant(*v) : QVariant();
}

bool ensureDirFor(const QString &filePath, QString *errorOut)
{
    const QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot create directory %1").arg(dir.absolutePath());
        return false;
    }
    return true;
}

QString escapeHtml(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&amp;"))
               .replace(QLatin1Char('<'), QStringLiteral("&lt;"))
               .replace(QLatin1Char('>'), QStringLiteral("&gt;"))
               .replace(QLatin1Char('"'), QStringLiteral("&quot;"));
}

} // namespace

bool Exporter::exportCsv(const QList<SampleRow> &samples, const QString &filePath,
                         QString *errorOut)
{
    if (!ensureDirFor(filePath, errorOut))
        return false;
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot open %1 for writing").arg(filePath);
        return false;
    }

    QTextStream out(&file);
    out << "timestamp_iso,timestamp_ms,instance,remaining_mwh,full_charged_mwh,"
           "charge_rate_mw,discharge_rate_mw,voltage_mv,percent,temperature_c,"
           "power_online,charging,discharging,cycle_count\n";
    for (const SampleRow &row : samples) {
        const QStringList fields{
            QDateTime::fromMSecsSinceEpoch(row.tsMs).toString(Qt::ISODate),
            QString::number(row.tsMs),
            csvField(row.instanceName),
            csvField(optVar(row.remainingmWh)),
            csvField(optVar(row.fullChargedmWh)),
            csvField(optVar(row.chargeRatemW)),
            csvField(optVar(row.dischargeRatemW)),
            csvField(optVar(row.voltagemV)),
            row.percent.has_value() ? QString::number(*row.percent, 'f', 2) : QString(),
            row.temperatureC.has_value() ? QString::number(*row.temperatureC, 'f', 2)
                                         : QString(),
            row.powerOnline.has_value() ? QString::number(*row.powerOnline ? 1 : 0) : QString(),
            row.charging.has_value() ? QString::number(*row.charging ? 1 : 0) : QString(),
            row.discharging.has_value() ? QString::number(*row.discharging ? 1 : 0) : QString(),
            csvField(optVar(row.cycleCount)),
        };
        out << fields.join(QLatin1Char(',')) << '\n';
    }
    out.flush();
    if (!file.commit()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to write %1").arg(filePath);
        return false;
    }
    return true;
}

bool Exporter::exportJson(const QList<SampleRow> &samples, const QString &filePath,
                          QString *errorOut)
{
    if (!ensureDirFor(filePath, errorOut))
        return false;

    QJsonArray array;
    for (const SampleRow &row : samples) {
        QJsonObject obj;
        obj.insert(QStringLiteral("timestamp"),
                   QDateTime::fromMSecsSinceEpoch(row.tsMs).toString(Qt::ISODate));
        obj.insert(QStringLiteral("timestampMs"), row.tsMs);
        obj.insert(QStringLiteral("instance"), row.instanceName);
        auto put = [&obj](const char *key, const QVariant &v) {
            if (v.isValid() && !v.isNull())
                obj.insert(QLatin1String(key), QJsonValue::fromVariant(v));
        };
        put("remainingmWh", optVar(row.remainingmWh));
        put("fullChargedmWh", optVar(row.fullChargedmWh));
        put("chargeRatemW", optVar(row.chargeRatemW));
        put("dischargeRatemW", optVar(row.dischargeRatemW));
        put("voltagemV", optVar(row.voltagemV));
        put("percent", optVar(row.percent));
        put("temperatureC", optVar(row.temperatureC));
        put("powerOnline", optVar(row.powerOnline));
        put("charging", optVar(row.charging));
        put("discharging", optVar(row.discharging));
        put("cycleCount", optVar(row.cycleCount));
        array.append(obj);
    }
    QJsonObject root;
    root.insert(QStringLiteral("generator"), QStringLiteral("Faraday"));
    root.insert(QStringLiteral("sampleCount"), array.size());
    root.insert(QStringLiteral("samples"), array);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot open %1 for writing").arg(filePath);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to write %1").arg(filePath);
        return false;
    }
    return true;
}

QString Exporter::buildHtmlReport(const ReportInput &input)
{
    QString html;
    html.reserve(32768);

    html += QStringLiteral(
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
        "<title>Faraday Battery Health Report</title>\n<style>\n"
        "body{font-family:'Segoe UI',sans-serif;background:#0d1117;color:#e6edf3;"
        "margin:0;padding:32px;}"
        ".wrap{max-width:860px;margin:0 auto;}"
        "h1{font-size:24px;margin:0 0 4px;}"
        ".dim{color:#8b949e;font-size:13px;}"
        ".card{background:#161b22;border:1px solid #30363d;border-radius:10px;"
        "padding:20px;margin-top:16px;}"
        ".grade{font-size:40px;font-weight:700;}"
        ".grid{display:flex;flex-wrap:wrap;gap:18px;margin-top:10px;}"
        ".metric{min-width:140px;}"
        ".metric .v{font-size:20px;font-weight:600;}"
        ".metric .l{font-size:11px;color:#8b949e;letter-spacing:1px;}"
        "table{border-collapse:collapse;width:100%;margin-top:8px;font-size:13px;}"
        "th,td{border-bottom:1px solid #30363d;padding:6px 8px;text-align:left;}"
        "th{color:#8b949e;font-weight:600;}"
        "ul{margin:8px 0 0 18px;padding:0;}li{margin:4px 0;font-size:14px;}"
        "</style>\n</head>\n<body>\n<div class=\"wrap\">\n");

    html += QStringLiteral("<h1>Faraday — Battery Health Report</h1>\n");
    html += QStringLiteral("<div class=\"dim\">Generated %1 · Faraday %2</div>\n")
                .arg(escapeHtml(input.generatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))),
                     escapeHtml(input.appVersion));

    // Verdict card
    html += QStringLiteral("<div class=\"card\">");
    if (input.healthPercent >= 0) {
        html += QStringLiteral("<div class=\"grade\" style=\"color:%1\">%2% — %3</div>")
                    .arg(escapeHtml(input.gradeColor))
                    .arg(input.healthPercent, 0, 'f', 1)
                    .arg(escapeHtml(input.gradeName));
    } else {
        html += QStringLiteral("<div class=\"grade\">Health unknown</div>");
    }
    html += QStringLiteral("<p>%1</p>").arg(escapeHtml(input.verdictHeadline));

    html += QStringLiteral("<div class=\"grid\">");
    auto metric = [&html](const QString &label, const QString &value) {
        html += QStringLiteral("<div class=\"metric\"><div class=\"v\">%1</div>"
                               "<div class=\"l\">%2</div></div>")
                    .arg(escapeHtml(value), escapeHtml(label));
    };
    if (input.designmWh > 0)
        metric(QStringLiteral("DESIGN CAPACITY"),
               QStringLiteral("%1 Wh").arg(input.designmWh / 1000.0, 0, 'f', 1));
    if (input.fullChargemWh > 0)
        metric(QStringLiteral("FULL CHARGE"),
               QStringLiteral("%1 Wh").arg(input.fullChargemWh / 1000.0, 0, 'f', 1));
    if (input.wearPercent >= 0)
        metric(QStringLiteral("WEAR"),
               QStringLiteral("%1%").arg(input.wearPercent, 0, 'f', 1));
    if (input.cycleCount >= 0)
        metric(QStringLiteral("CYCLES"), QString::number(input.cycleCount));
    if (!input.chemistry.isEmpty())
        metric(QStringLiteral("CHEMISTRY"), input.chemistry);
    html += QStringLiteral("</div></div>\n");

    // Battery identity
    html += QStringLiteral("<div class=\"card\"><b>Battery</b><table>");
    auto row = [&html](const QString &k, const QString &v) {
        if (!v.isEmpty())
            html += QStringLiteral("<tr><th>%1</th><td>%2</td></tr>")
                        .arg(escapeHtml(k), escapeHtml(v));
    };
    row(QStringLiteral("Name"), input.batteryName);
    row(QStringLiteral("Manufacturer"), input.manufacturer);
    row(QStringLiteral("Serial number"), input.serialNumber);
    row(QStringLiteral("Chemistry"), input.chemistry);
    html += QStringLiteral("</table></div>\n");

    // Observations
    if (!input.verdictDetails.isEmpty() || !input.insights.isEmpty()) {
        html += QStringLiteral("<div class=\"card\"><b>Observations</b><ul>");
        for (const QString &d : input.verdictDetails)
            html += QStringLiteral("<li>%1</li>").arg(escapeHtml(d));
        for (const QString &i : input.insights)
            html += QStringLiteral("<li>%1</li>").arg(escapeHtml(i));
        html += QStringLiteral("</ul></div>\n");
    }

    // Capacity history: inline SVG chart + table
    QList<CapacityHistoryRow> points;
    for (const CapacityHistoryRow &h : input.history) {
        if (h.fullChargemWh.value_or(0) > 0 && h.startDate.isValid())
            points.append(h);
    }
    if (!points.isEmpty()) {
        html += QStringLiteral("<div class=\"card\"><b>Capacity history</b>");

        const int W = 800, H = 220, padL = 60, padR = 12, padT = 14, padB = 26;
        const qint64 t0 = QDateTime(points.first().startDate, QTime(0, 0)).toSecsSinceEpoch();
        qint64 t1 = QDateTime(points.last().startDate, QTime(0, 0)).toSecsSinceEpoch();
        if (t1 <= t0)
            t1 = t0 + 86400;
        double yMax = 0;
        for (const auto &p : points)
            yMax = qMax(yMax, double(qMax(p.fullChargemWh.value_or(0),
                                          p.designmWh.value_or(0))));
        yMax *= 1.06;

        auto xOf = [&](qint64 t) {
            return padL + double(t - t0) / double(t1 - t0) * (W - padL - padR);
        };
        auto yOf = [&](double v) { return padT + (yMax - v) / yMax * (H - padT - padB); };

        QString poly;
        for (const auto &p : points) {
            const qint64 t = QDateTime(p.startDate, QTime(0, 0)).toSecsSinceEpoch();
            poly += QStringLiteral("%1,%2 ")
                        .arg(xOf(t), 0, 'f', 1)
                        .arg(yOf(double(*p.fullChargemWh)), 0, 'f', 1);
        }

        html += QStringLiteral("<svg viewBox=\"0 0 %1 %2\" width=\"100%\" "
                               "xmlns=\"http://www.w3.org/2000/svg\">")
                    .arg(W).arg(H);
        for (int i = 0; i <= 4; ++i) {
            const double v = yMax * i / 4.0;
            html += QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\" "
                                   "stroke=\"#30363d\" stroke-width=\"1\"/>")
                        .arg(padL).arg(yOf(v), 0, 'f', 1).arg(W - padR);
            html += QStringLiteral("<text x=\"%1\" y=\"%2\" fill=\"#8b949e\" "
                                   "font-size=\"10\" text-anchor=\"end\">%3 Wh</text>")
                        .arg(padL - 6).arg(yOf(v) + 3, 0, 'f', 1)
                        .arg(v / 1000.0, 0, 'f', 1);
        }
        const double designMax = [&points] {
            double d = 0;
            for (const auto &p : points)
                d = qMax(d, double(p.designmWh.value_or(0)));
            return d;
        }();
        if (designMax > 0) {
            html += QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%2\" "
                                   "stroke=\"#8b949e\" stroke-dasharray=\"4 5\"/>")
                        .arg(padL).arg(yOf(designMax), 0, 'f', 1).arg(W - padR);
        }
        html += QStringLiteral("<polyline points=\"%1\" fill=\"none\" stroke=\"#58a6ff\" "
                               "stroke-width=\"2\"/>")
                    .arg(poly.trimmed());
        html += QStringLiteral("</svg>");

        html += QStringLiteral("<table><tr><th>Week of</th><th>Full charge</th>"
                               "<th>Design</th><th>Cycles</th></tr>");
        for (const auto &p : points) {
            html += QStringLiteral("<tr><td>%1</td><td>%2 mWh</td><td>%3</td><td>%4</td></tr>")
                        .arg(p.startDate.toString(Qt::ISODate))
                        .arg(*p.fullChargemWh)
                        .arg(p.designmWh.has_value() ? QString::number(*p.designmWh)
                                                     : QStringLiteral("—"))
                        .arg(p.cycleCount.has_value() ? QString::number(*p.cycleCount)
                                                      : QStringLiteral("—"));
        }
        html += QStringLiteral("</table></div>\n");
    }

    html += QStringLiteral("<div class=\"dim\" style=\"margin-top:16px\">"
                           "Faraday — Battery Intelligence Suite. Generated locally; "
                           "this report contains no external resources.</div>\n");
    html += QStringLiteral("</div>\n</body>\n</html>\n");
    return html;
}

bool Exporter::writeHtmlReport(const ReportInput &input, const QString &filePath,
                               QString *errorOut)
{
    if (!ensureDirFor(filePath, errorOut))
        return false;
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Cannot open %1 for writing").arg(filePath);
        return false;
    }
    file.write(buildHtmlReport(input).toUtf8());
    if (!file.commit()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to write %1").arg(filePath);
        return false;
    }
    return true;
}

} // namespace faraday
