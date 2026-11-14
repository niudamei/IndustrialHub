#include "src/plugins/iplugin.h"

#include <QObject>

class EchoCommunication final : public ICommunication
{
public:
    explicit EchoCommunication(QObject *parent = nullptr) : ICommunication(parent) {}

    bool open(const DeviceInfo &, QString *errorMessage) override
    {
        m_open = true;
        if (errorMessage) errorMessage->clear();
        emit connectionStateChanged(true);
        return true;
    }
    void close() override
    {
        if (!m_open) return;
        m_open = false;
        emit connectionStateChanged(false);
    }
    bool isOpen() const override { return m_open; }
    bool sendData(const QByteArray &data, QString *errorMessage) override
    {
        if (!m_open) {
            if (errorMessage) *errorMessage = QStringLiteral("echo driver is closed");
            return false;
        }
        if (errorMessage) errorMessage->clear();
        emit dataReceived(data);
        return true;
    }
private:
    bool m_open = false;
};

class SampleIndustrialPlugin final : public QObject,
                                     public IIndustrialPlugin,
                                     public ICommunicationDriverPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID INDUSTRIALHUB_PLUGIN_IID FILE "sampleplugin.json")
    Q_INTERFACES(IIndustrialPlugin ICommunicationDriverPlugin)
public:
    QString pluginId() const override { return QStringLiteral("test.sample"); }
    QString displayName() const override { return QStringLiteral("IndustrialHub Sample Plugin"); }
    QString version() const override { return QStringLiteral("1.1.0"); }
    bool initialize(QString *errorMessage) override
    {
        if (errorMessage) errorMessage->clear();
        m_initialized = true;
        return true;
    }
    void shutdown() override { m_initialized = false; }
    QStringList communicationTypes() const override
    {
        return {QStringLiteral("PLUGIN-ECHO")};
    }
    ICommunication *createCommunication(const QString &communicationType,
                                        QObject *parent) override
    {
        if (!m_initialized || communicationType.compare(
                QStringLiteral("PLUGIN-ECHO"), Qt::CaseInsensitive) != 0)
            return nullptr;
        return new EchoCommunication(parent);
    }
private:
    bool m_initialized = false;
};

#include "sampleplugin.moc"
