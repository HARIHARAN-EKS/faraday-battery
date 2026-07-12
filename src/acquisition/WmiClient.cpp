#include "WmiClient.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif
#include <windows.h>
#include <wbemidl.h>
#include <oleauto.h>

namespace faraday {

namespace {

QString hresultToString(HRESULT hr)
{
    return QStringLiteral("0x%1").arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
}

QVariant variantToQVariant(const VARIANT &v)
{
    switch (v.vt) {
    case VT_EMPTY:
    case VT_NULL:
        return QVariant();
    case VT_BSTR:
        return v.bstrVal ? QString::fromWCharArray(v.bstrVal) : QString();
    case VT_BOOL:
        return v.boolVal != VARIANT_FALSE;
    case VT_I1:
        return static_cast<int>(v.cVal);
    case VT_UI1:
        return static_cast<uint>(v.bVal);
    case VT_I2:
        return static_cast<int>(v.iVal);
    case VT_UI2:
        return static_cast<uint>(v.uiVal);
    case VT_I4:
        return static_cast<qint32>(v.lVal);
    case VT_UI4:
        return static_cast<quint32>(v.ulVal);
    case VT_INT:
        return static_cast<qint32>(v.intVal);
    case VT_UINT:
        return static_cast<quint32>(v.uintVal);
    case VT_I8:
        return static_cast<qint64>(v.llVal);
    case VT_UI8:
        return static_cast<quint64>(v.ullVal);
    case VT_R4:
        return static_cast<double>(v.fltVal);
    case VT_R8:
        return v.dblVal;
    default:
        // Arrays / references / dates are not needed by any reader; skip them.
        return QVariant();
    }
}

} // namespace

WmiClient::~WmiClient()
{
    release();
    if (m_comInitialized)
        CoUninitialize();
}

void WmiClient::release()
{
    if (m_services) {
        m_services->Release();
        m_services = nullptr;
    }
    if (m_locator) {
        m_locator->Release();
        m_locator = nullptr;
    }
}

bool WmiClient::connect(const QString &wmiNamespace)
{
    release();

    if (!m_comInitialized) {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == S_OK || hr == S_FALSE) {
            m_comInitialized = true;
        } else if (hr == RPC_E_CHANGED_MODE) {
            // COM already initialized on this thread with a different model;
            // that is fine for our purposes, but we must not CoUninitialize.
        } else {
            m_lastError = QStringLiteral("CoInitializeEx failed: %1").arg(hresultToString(hr));
            return false;
        }
    }

    // Process-wide; harmless if it was already called (RPC_E_TOO_LATE).
    const HRESULT hrSec = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);
    if (FAILED(hrSec) && hrSec != RPC_E_TOO_LATE) {
        m_lastError = QStringLiteral("CoInitializeSecurity failed: %1").arg(hresultToString(hrSec));
        return false;
    }

    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWbemLocator, reinterpret_cast<void **>(&m_locator));
    if (FAILED(hr) || !m_locator) {
        m_locator = nullptr;
        m_lastError = QStringLiteral("CoCreateInstance(WbemLocator) failed: %1").arg(hresultToString(hr));
        return false;
    }

    BSTR ns = SysAllocString(reinterpret_cast<const wchar_t *>(wmiNamespace.utf16()));
    if (!ns) {
        m_lastError = QStringLiteral("SysAllocString failed (out of memory)");
        release();
        return false;
    }
    // WBEM_FLAG_CONNECT_USE_MAX_WAIT: fail within ~2 minutes instead of
    // blocking indefinitely if the WMI service is wedged.
    hr = m_locator->ConnectServer(ns, nullptr, nullptr, nullptr,
                                  WBEM_FLAG_CONNECT_USE_MAX_WAIT, nullptr, nullptr, &m_services);
    SysFreeString(ns);
    if (FAILED(hr) || !m_services) {
        m_services = nullptr;
        m_lastError = QStringLiteral("ConnectServer(%1) failed: %2")
                          .arg(wmiNamespace, hresultToString(hr));
        release();
        return false;
    }

    hr = CoSetProxyBlanket(m_services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        m_lastError = QStringLiteral("CoSetProxyBlanket failed: %1").arg(hresultToString(hr));
        release();
        return false;
    }

    m_lastError.clear();
    return true;
}

bool WmiClient::drainEnumerator(IEnumWbemClassObject *enumerator, const QString &context,
                                QList<QVariantMap> &rows)
{
    bool enumerationFailed = false;
    for (;;) {
        IWbemClassObject *obj = nullptr;
        ULONG returned = 0;
        const HRESULT hr = enumerator->Next(static_cast<long>(WBEM_INFINITE), 1, &obj, &returned);
        if (FAILED(hr)) {
            // e.g. WBEM_E_INVALID_CLASS for a missing class, or the
            // intermittent WBEM_E_FAILED from perf-adapter classes.
            m_lastError = QStringLiteral("Enumeration failed for '%1': %2")
                              .arg(context, hresultToString(hr));
            enumerationFailed = true;
            break;
        }
        if (hr == WBEM_S_FALSE || returned == 0)
            break;
        if (!obj)
            continue;

        QVariantMap row;
        if (SUCCEEDED(obj->BeginEnumeration(WBEM_FLAG_LOCAL_ONLY))) {
            for (;;) {
                BSTR propName = nullptr;
                VARIANT value;
                VariantInit(&value);
                const HRESULT hrNext = obj->Next(0, &propName, &value, nullptr, nullptr);
                if (hrNext != WBEM_S_NO_ERROR) {
                    VariantClear(&value);
                    if (propName)
                        SysFreeString(propName);
                    break;
                }
                if (propName) {
                    const QVariant qv = variantToQVariant(value);
                    if (qv.isValid())
                        row.insert(QString::fromWCharArray(propName), qv);
                    SysFreeString(propName);
                }
                VariantClear(&value);
            }
            obj->EndEnumeration();
        }
        obj->Release();
        rows.append(row);
    }
    enumerator->Release();
    return !enumerationFailed;
}

QList<QVariantMap> WmiClient::query(const QString &wql, bool *ok)
{
    QList<QVariantMap> results;
    if (ok)
        *ok = false;

    if (!m_services) {
        m_lastError = QStringLiteral("query() called without a WMI connection");
        return results;
    }

    BSTR language = SysAllocString(L"WQL");
    BSTR queryText = SysAllocString(reinterpret_cast<const wchar_t *>(wql.utf16()));
    if (!language || !queryText) {
        if (language)
            SysFreeString(language);
        if (queryText)
            SysFreeString(queryText);
        m_lastError = QStringLiteral("SysAllocString failed (out of memory)");
        return results;
    }

    IEnumWbemClassObject *enumerator = nullptr;
    HRESULT hr = m_services->ExecQuery(
        language, queryText,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &enumerator);
    SysFreeString(language);
    SysFreeString(queryText);

    if (FAILED(hr) || !enumerator) {
        m_lastError = QStringLiteral("ExecQuery failed for '%1': %2").arg(wql, hresultToString(hr));
        return results;
    }

    if (!drainEnumerator(enumerator, wql, results))
        return results;

    if (ok)
        *ok = true;
    m_lastError.clear();
    return results;
}

QList<QVariantMap> WmiClient::queryInstances(const QString &className, bool *ok)
{
    bool queryOk = false;
    QList<QVariantMap> results =
        query(QStringLiteral("SELECT * FROM %1").arg(className), &queryOk);
    if (queryOk) {
        if (ok)
            *ok = true;
        return results;
    }

    if (ok)
        *ok = false;
    if (!m_services)
        return {};

    // Retry through direct instance enumeration with a rewindable
    // (non-forward-only) enumerator; see the header for why this can
    // succeed where the query path intermittently fails.
    const QString firstError = m_lastError;

    BSTR cls = SysAllocString(reinterpret_cast<const wchar_t *>(className.utf16()));
    if (!cls) {
        m_lastError = QStringLiteral("SysAllocString failed (out of memory)");
        return {};
    }

    IEnumWbemClassObject *enumerator = nullptr;
    const HRESULT hr =
        m_services->CreateInstanceEnum(cls, WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    SysFreeString(cls);

    if (FAILED(hr) || !enumerator) {
        m_lastError = QStringLiteral("%1; CreateInstanceEnum retry failed: %2")
                          .arg(firstError, hresultToString(hr));
        return {};
    }

    QList<QVariantMap> retryResults;
    if (!drainEnumerator(enumerator, className, retryResults)) {
        m_lastError = QStringLiteral("%1; retry: %2").arg(firstError, m_lastError);
        return {};
    }

    if (ok)
        *ok = true;
    m_lastError.clear();
    return retryResults;
}

} // namespace faraday
