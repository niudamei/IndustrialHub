#include "slcanframecodec.h"

namespace {
bool isHexString(const QByteArray &value)
{
    for (char ch : value) {
        const bool digit = ch >= '0' && ch <= '9';
        const bool upper = ch >= 'A' && ch <= 'F';
        const bool lower = ch >= 'a' && ch <= 'f';
        if (!digit && !upper && !lower)
            return false;
    }
    return true;
}
}

bool SlcanFrameCodec::parseFrame(const QByteArray &input,
                                 bool canFdEnabled,
                                 SlcanFrame &frame,
                                 QString *errorMessage)
{
    QByteArray command = input;
    while (!command.isEmpty()
           && (command.endsWith('\r') || command.endsWith('\n'))) {
        command.chop(1);
    }

    if (command.contains('\r') || command.contains('\n') || command.contains(' ')) {
        return setError(QStringLiteral("SLCAN 帧不能包含空格或内部换行。"), errorMessage);
    }

    if (command.isEmpty())
        return setError(QStringLiteral("SLCAN 帧不能为空。"), errorMessage);

    const char type = command.at(0);
    const bool standardClassic = type == 't';
    const bool extendedClassic = type == 'T';
    const bool standardRemote = type == 'r';
    const bool extendedRemote = type == 'R';
    const bool standardFd = type == 'd' || type == 'b';
    const bool extendedFd = type == 'D' || type == 'B';

    if (!(standardClassic || extendedClassic || standardRemote || extendedRemote
          || standardFd || extendedFd)) {
        return setError(QStringLiteral("不支持的 SLCAN 帧类型：%1")
                            .arg(QChar::fromLatin1(type)), errorMessage);
    }

    const bool extended = extendedClassic || extendedRemote || extendedFd;
    const bool remote = standardRemote || extendedRemote;
    const bool fd = standardFd || extendedFd;
    const bool brs = type == 'b' || type == 'B';

    if (fd && !canFdEnabled)
        return setError(QStringLiteral("当前设备配置为经典 CAN，不允许发送 CAN-FD 帧。"),
                        errorMessage);

    if (command.size() < (extended ? 10 : 5))
        return setError(QStringLiteral("SLCAN 帧长度不足。"), errorMessage);

    const int idWidth = extended ? 8 : 3;
    const QByteArray idText = command.mid(1, idWidth);
    const QByteArray dlcText = command.mid(1 + idWidth, 1);
    if (!isHexString(idText) || !isHexString(dlcText))
        return setError(QStringLiteral("SLCAN ID 或 DLC 包含非十六进制字符。"),
                        errorMessage);

    bool idOk = false;
    const quint32 id = idText.toUInt(&idOk, 16);
    const quint32 maxId = extended ? 0x1FFFFFFF : 0x7FF;
    if (!idOk || id > maxId)
        return setError(QStringLiteral("CAN ID 超出标准帧/扩展帧允许范围。"),
                        errorMessage);

    bool dlcOk = false;
    const int dlc = dlcText.toInt(&dlcOk, 16);
    if (!dlcOk)
        return setError(QStringLiteral("SLCAN DLC 无效。"), errorMessage);

    const int dataBytes = bytesFromDlc(dlc, fd);
    if (dataBytes < 0)
        return setError(fd
                            ? QStringLiteral("CAN-FD DLC 必须在 0-F 范围内。")
                            : QStringLiteral("经典 CAN DLC 必须在 0-8 范围内。"),
                        errorMessage);

    const int expectedHexChars = remote ? 0 : dataBytes * 2;
    const QByteArray dataText = command.mid(2 + idWidth);
    if (dataText.size() != expectedHexChars || !isHexString(dataText))
        return setError(QStringLiteral("SLCAN 数据长度与 DLC 不匹配，或数据包含非法字符。"),
                        errorMessage);

    if (remote && fd)
        return setError(QStringLiteral("CAN-FD 不支持远程帧。"), errorMessage);

    frame.id = id;
    frame.extended = extended;
    frame.fd = fd;
    frame.brs = brs;
    frame.remote = remote;
    frame.data = remote ? QByteArray() : QByteArray::fromHex(dataText);
    return true;
}

bool SlcanFrameCodec::encodeFrame(const SlcanFrame &frame,
                                  QByteArray &command,
                                  QString *errorMessage)
{
    if (frame.id > (frame.extended ? 0x1FFFFFFF : 0x7FF))
        return setError(QStringLiteral("CAN ID 超出标准帧/扩展帧允许范围。"),
                        errorMessage);

    if (frame.fd && frame.remote)
        return setError(QStringLiteral("CAN-FD 不支持远程帧。"), errorMessage);

    if (!frame.fd && frame.brs)
        return setError(QStringLiteral("经典 CAN 帧不能启用 BRS。"), errorMessage);

    if (frame.remote && !frame.data.isEmpty())
        return setError(QStringLiteral("远程帧不能携带数据。"), errorMessage);

    const int dlc = dlcFromBytes(frame.remote ? 0 : frame.data.size(), frame.fd);
    if (dlc < 0)
        return setError(frame.fd
                            ? QStringLiteral("CAN-FD 数据长度必须为 0-8、12、16、20、24、32、48 或 64 字节。")
                            : QStringLiteral("经典 CAN 数据长度不能超过 8 字节。"),
                        errorMessage);

    char type = 't';
    if (frame.remote)
        type = frame.extended ? 'R' : 'r';
    else if (frame.fd)
        type = frame.brs ? (frame.extended ? 'B' : 'b')
                         : (frame.extended ? 'D' : 'd');
    else
        type = frame.extended ? 'T' : 't';

    const int idWidth = frame.extended ? 8 : 3;
    const QByteArray idText = QByteArray::number(frame.id, 16)
                                  .toUpper().rightJustified(idWidth, '0');
    const char dlcChar = "0123456789ABCDEF"[dlc];

    command.clear();
    command.append(type);
    command.append(idText);
    command.append(dlcChar);
    if (!frame.remote)
        command.append(frame.data.toHex().toUpper());
    return true;
}

bool SlcanFrameCodec::normalizeCommand(const QByteArray &command,
                                       bool canFdEnabled,
                                       QByteArray &normalized,
                                       QString *errorMessage)
{
    SlcanFrame frame;
    if (!parseFrame(command, canFdEnabled, frame, errorMessage))
        return false;

    if (!encodeFrame(frame, normalized, errorMessage))
        return false;

    normalized.append('\r');
    return true;
}

int SlcanFrameCodec::bytesFromDlc(int dlc, bool fd)
{
    if (dlc < 0 || dlc > (fd ? 15 : 8))
        return -1;

    if (!fd || dlc <= 8)
        return dlc;

    static const int fdLengths[] = {12, 16, 20, 24, 32, 48, 64};
    return fdLengths[dlc - 9];
}

int SlcanFrameCodec::dlcFromBytes(int bytes, bool fd)
{
    if (bytes < 0)
        return -1;

    if (bytes <= 8)
        return bytes;

    if (!fd)
        return -1;

    switch (bytes) {
    case 12: return 9;
    case 16: return 10;
    case 20: return 11;
    case 24: return 12;
    case 32: return 13;
    case 48: return 14;
    case 64: return 15;
    default: return -1;
    }
}

bool SlcanFrameCodec::setError(const QString &message, QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = message;
    return false;
}
