#include <QCoreApplication>
#include <QDebug>
#include <QVector>

#include "src/protocol/jsonprotocolparser.h"
#include "src/protocol/protocolmanager.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        qCritical() << "CHECK failed:" << #condition << "at line" << __LINE__; \
        return 1; \
    } \
} while (false)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto parser = std::make_shared<JsonProtocolParser>();
    QVector<BusinessDataPoint> points;
    QString error;

    const QByteArray validPayload = R"({
        "timestamp":"2026-11-13T10:20:30.123Z",
        "values":[
            {"id":"temperature","name":"温度","value":26.5,"unit":"°C","quality":"good"},
            {"id":"rpm","name":"转速","value":1200,"unit":"rpm","quality":"uncertain"},
            {"id":"current","value":2.35,"unit":"A","quality":"bad"}
        ]
    })";

    CHECK(parser->parse(QStringLiteral("DEV-JSON-01"), validPayload, points, &error));
    CHECK(error.isEmpty());
    CHECK(points.size() == 3);
    CHECK(points.at(0).deviceId == QStringLiteral("DEV-JSON-01"));
    CHECK(points.at(0).variableId == QStringLiteral("temperature"));
    CHECK(points.at(0).name == QStringLiteral("温度"));
    CHECK(qAbs(points.at(0).value - 26.5) < 0.000001);
    CHECK(points.at(0).unit == QStringLiteral("°C"));
    CHECK(points.at(0).quality == DataQuality::Good);
    CHECK(points.at(1).quality == DataQuality::Uncertain);
    CHECK(points.at(2).quality == DataQuality::Bad);
    CHECK(points.at(0).timestamp.isValid());
    CHECK(points.at(0).sourceProtocol == QStringLiteral("JSON"));

    ProtocolManager manager;
    CHECK(manager.registerParser(QStringLiteral("json"), parser, &error));
    CHECK(manager.bindDevice(QStringLiteral("dev-json-01"), QStringLiteral("JSON"), &error));
    CHECK(manager.parserForDevice(QStringLiteral("DEV-JSON-01")) == QStringLiteral("json"));

    int decodedSignalCount = 0;
    QObject::connect(&manager, &ProtocolManager::businessDataDecoded,
                     [&decodedSignalCount](const QString &deviceId,
                                           const QVector<BusinessDataPoint> &decoded) {
        if (deviceId == QStringLiteral("DEV-JSON-01") && decoded.size() == 3)
            ++decodedSignalCount;
    });
    CHECK(manager.parseFrame(QStringLiteral("DEV-JSON-01"), validPayload, &error));
    CHECK(decodedSignalCount == 1);

    const QVector<BusinessDataPoint> previousPoints = points;
    CHECK(!parser->parse(QStringLiteral("DEV-JSON-01"),
                         QByteArray("{invalid json"), points, &error));
    CHECK(!error.isEmpty());
    CHECK(points.size() == previousPoints.size());

    CHECK(!parser->parse(QStringLiteral("DEV-JSON-01"),
                         QByteArray(R"({"values":[{"id":"x","value":"not-number"}]})"),
                         points, &error));
    CHECK(!error.isEmpty());

    CHECK(!parser->parse(QString(), validPayload, points, &error));
    CHECK(!manager.parseFrame(QStringLiteral("UNBOUND-DEVICE"), validPayload, &error));

    qInfo() << "JSON protocol parser tests passed.";
    return 0;
}
