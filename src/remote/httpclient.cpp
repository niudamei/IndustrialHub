#include "httpclient.h"

#include <QNetworkReply>
#include <QNetworkRequest>

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        if (!m_reply)
            return;
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
        emit requestFailed(QStringLiteral("HTTP 请求超时。"));
    });
}

bool HttpClient::isBusy() const
{
    return !m_reply.isNull();
}

bool HttpClient::sendRequest(const QString &method, const QUrl &url,
                             const QByteArray &body, const QString &bearerToken,
                             int timeoutMs)
{
    if (isBusy()) {
        emit requestFailed(QStringLiteral("已有 HTTP 请求正在执行。"));
        return false;
    }
    const QString verb = method.trimmed().toUpper();
    if (verb != QStringLiteral("GET") && verb != QStringLiteral("POST")
        && verb != QStringLiteral("PUT") && verb != QStringLiteral("PATCH")
        && verb != QStringLiteral("DELETE")) {
        emit requestFailed(QStringLiteral("不支持的 HTTP 方法。"));
        return false;
    }
    if (!url.isValid() || (url.scheme() != QStringLiteral("http")
        && url.scheme() != QStringLiteral("https")) || url.host().isEmpty()) {
        emit requestFailed(QStringLiteral("HTTP URL 无效，必须使用 http:// 或 https://。"));
        return false;
    }
    if (body.size() > 1024 * 1024) {
        emit requestFailed(QStringLiteral("HTTP 请求体超过 1 MiB 限制。"));
        return false;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json, */*");
    request.setTransferTimeout(qBound(1000, timeoutMs, 120000));
    if (!bearerToken.trimmed().isEmpty())
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + bearerToken.trimmed().toUtf8());

    if (verb == QStringLiteral("GET"))
        m_reply = m_manager.get(request);
    else if (verb == QStringLiteral("POST"))
        m_reply = m_manager.post(request, body);
    else if (verb == QStringLiteral("PUT"))
        m_reply = m_manager.put(request, body);
    else if (verb == QStringLiteral("PATCH"))
        m_reply = m_manager.sendCustomRequest(request, "PATCH", body);
    else if (body.isEmpty())
        m_reply = m_manager.deleteResource(request);
    else
        m_reply = m_manager.sendCustomRequest(request, "DELETE", body);

    if (!m_reply) {
        emit requestFailed(QStringLiteral("无法创建 HTTP 请求。"));
        return false;
    }

    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_reply != reply)
            return;
        m_timeout.stop();
        m_reply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray response = reply->readAll();
        const QString networkError = reply->error() == QNetworkReply::NoError
            || status > 0 ? QString() : reply->errorString();
        reply->deleteLater();
        if (!networkError.isEmpty()) {
            emit requestFailed(networkError);
            return;
        }
        if (response.size() > 1024 * 1024) {
            emit requestFailed(QStringLiteral("HTTP 响应超过 1 MiB 限制。"));
            return;
        }
        emit requestFinished(status, response);
    });
    m_timeout.start(qBound(1000, timeoutMs, 120000));
    return true;
}

void HttpClient::cancel()
{
    m_timeout.stop();
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->abort();
    reply->deleteLater();
}

