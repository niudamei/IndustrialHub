#ifndef SLCANFRAMECODEC_H
#define SLCANFRAMECODEC_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

struct SlcanFrame
{
    quint32 id = 0;
    bool extended = false;
    bool fd = false;
    bool brs = false;
    bool remote = false;
    quint8 remoteDlc = 0;
    QByteArray data;
};

// CANable SLCAN / CAN-FD 文本帧编解码。
// 输入/输出命令不包含串口 CR 终止符，normalizeCommand 会在输出末尾补 CR。
class SlcanFrameCodec
{
public:
    static bool parseFrame(const QByteArray &command,
                           bool canFdEnabled,
                           SlcanFrame &frame,
                           QString *errorMessage = nullptr);

    static bool encodeFrame(const SlcanFrame &frame,
                            QByteArray &command,
                            QString *errorMessage = nullptr);

    static bool normalizeCommand(const QByteArray &command,
                                 bool canFdEnabled,
                                 QByteArray &normalized,
                                 QString *errorMessage = nullptr);

private:
    static int bytesFromDlc(int dlc, bool fd);
    static int dlcFromBytes(int bytes, bool fd);
    static bool setError(const QString &message, QString *errorMessage);
};

#endif // SLCANFRAMECODEC_H
