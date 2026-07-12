#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

struct IWbemLocator;
struct IWbemServices;
struct IEnumWbemClassObject;

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

    // Enumerates all instances of a class ("SELECT * FROM <className>").
    // Tries ExecQuery first; if the provider fails mid-enumeration it retries
    // through IWbemServices::CreateInstanceEnum with a rewindable enumerator.
    // Rationale: classes in the Win32_PerfRawData hierarchy (BatteryStaticData
    // et al.) are served through the performance adapter, which intermittently
    // returns WBEM_E_FAILED (0x80041001) on the query path while direct
    // instance enumeration succeeds — observed and verified on real hardware.
    QList<QVariantMap> queryInstances(const QString &className, bool *ok = nullptr);

private:
    void release();
    // Drains an enumerator into rows. Returns false (and sets m_lastError)
    // if enumeration itself fails; the enumerator is always released.
    bool drainEnumerator(IEnumWbemClassObject *enumerator, const QString &context,
                         QList<QVariantMap> &rows);

    IWbemLocator *m_locator = nullptr;
    IWbemServices *m_services = nullptr;
    bool m_comInitialized = false;
    QString m_lastError;
};

} // namespace faraday
