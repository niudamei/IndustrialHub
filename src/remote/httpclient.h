#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

class HttpClient : public QObject
{
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);
    bool isBusy() const;
    bool sendRequest(const QString &method, const QUrl &url,
                     const QByteArray &body = QByteArray(),
                     const QString &bearerToken = QString(),
                     int timeoutMs = 10000);
    void cancel();

signals:
    void requestFinished(int statusCode, const QByteArray &body);
    void requestFailed(const QString &message);

private:
    QNetworkAccessManager m_manager;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timeout;
};

#endif // HTTPCLIENT_H
