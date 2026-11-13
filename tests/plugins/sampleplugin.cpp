#include "src/plugins/iplugin.h"

#include <QObject>

class SampleIndustrialPlugin final : public QObject, public IIndustrialPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID INDUSTRIALHUB_PLUGIN_IID FILE "sampleplugin.json")
    Q_INTERFACES(IIndustrialPlugin)
public:
    QString pluginId() const override { return QStringLiteral("test.sample"); }
    QString displayName() const override { return QStringLiteral("IndustrialHub Sample Plugin"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    bool initialize(QString *errorMessage) override
    {
        if (errorMessage) errorMessage->clear();
        m_initialized = true;
        return true;
    }
    void shutdown() override { m_initialized = false; }
private:
    bool m_initialized = false;
};

#include "sampleplugin.moc"
