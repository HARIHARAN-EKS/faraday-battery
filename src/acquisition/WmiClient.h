#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

struct IWbemLocator;
struct IWbemServices;

namespace faraday {

// Thin, read-only WMI query client over COM.
//
// Thread affinity: connect() and query() must be called on the same thread.
// COM is initialized lazily on first connect() and released in the destructor.
// All failures are reported through return values / lastError(); nothing throws.
class WmiClient
{
public:
    WmiClient() = default;
    ~WmiClient();

    WmiClient(const WmiClient &) = delete;
    WmiClient &operator=(const WmiClient &) = delete;

    // wmiNamespace example: "ROOT\\WMI" or "ROOT\\CIMV2"
    bool connect(const QString &wmiNamespace);
    bool isConnected() const { return m_services != nullptr; }
    QString lastError() const { return m_lastError; }

    // Executes a WQL SELECT. Returns one map per instance (property -> value).
    // *ok is false on any COM/WMI error (including errors mid-enumeration);
    // an existing class with zero instances yields *ok == true and an empty list.
    QList<QVariantMap> query(const QString &wql, bool *ok = nullptr);

private:
    void release();

    IWbemLocator *m_locator = nullptr;
    IWbemServices *m_services = nullptr;
    bool m_comInitialized = false;
    QString m_lastError;
};

} // namespace faraday
