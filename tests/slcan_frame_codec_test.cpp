#include "src/communication/slcanframecodec.h"

#include <iostream>

int main()
{
    QString error;
    SlcanFrame frame;

    if (!SlcanFrameCodec::parseFrame("T18AABBCC03112233", false, frame, &error)
        || frame.id != 0x18AABBCC || !frame.extended || frame.fd
        || frame.data != QByteArray::fromHex("112233")) {
        std::cerr << "FAIL: extended classic CAN parse: " << error.toStdString() << std::endl;
        return 1;
    }

    QByteArray encoded;
    if (!SlcanFrameCodec::encodeFrame(frame, encoded, &error)
        || encoded != QByteArray("T18AABBCC03112233")) {
        std::cerr << "FAIL: classic CAN encode: " << error.toStdString() << std::endl;
        return 2;
    }
    std::cout << "PASS: classic extended CAN frame codec." << std::endl;

    if (!SlcanFrameCodec::parseFrame("d7E09112233445566778899AABBCC", true,
                                     frame, &error)
        || !frame.fd || frame.extended || frame.brs
        || frame.data.size() != 12) {
        std::cerr << "FAIL: CAN-FD DLC mapping: " << error.toStdString() << std::endl;
        return 3;
    }

    if (!SlcanFrameCodec::normalizeCommand("d7e09112233445566778899aabbcc",
                                            true, encoded, &error)
        || encoded != QByteArray("d7E09112233445566778899AABBCC\r")) {
        std::cerr << "FAIL: CAN-FD normalization: " << error.toStdString() << std::endl;
        return 4;
    }
    std::cout << "PASS: CAN-FD DLC and normalization." << std::endl;

    if (SlcanFrameCodec::parseFrame("d7E09112233445566778899AABBCC",
                                    false, frame, &error)) {
        std::cerr << "FAIL: CAN-FD frame accepted in classic CAN mode." << std::endl;
        return 5;
    }

    if (SlcanFrameCodec::parseFrame("t1239AABBCCDDEEFF00",
                                    false, frame, &error)) {
        std::cerr << "FAIL: classic CAN accepted DLC 9." << std::endl;
        return 6;
    }

    if (SlcanFrameCodec::parseFrame("T18AABBCC04112233",
                                    false, frame, &error)) {
        std::cerr << "FAIL: mismatched payload length was accepted." << std::endl;
        return 7;
    }

    SlcanFrame fdFrame;
    fdFrame.id = 0x7E0;
    fdFrame.fd = true;
    fdFrame.brs = true;
    fdFrame.data = QByteArray::fromHex("112233445566778899AABBCC");
    if (!SlcanFrameCodec::encodeFrame(fdFrame, encoded, &error)
        || encoded != QByteArray("b7E09112233445566778899AABBCC")) {
        std::cerr << "FAIL: CAN-FD BRS encode: " << error.toStdString() << std::endl;
        return 8;
    }

    SlcanFrame remoteFrame;
    if (!SlcanFrameCodec::parseFrame("R000001230", false, remoteFrame, &error)
        || !remoteFrame.remote || remoteFrame.remoteDlc != 0
        || !SlcanFrameCodec::encodeFrame(remoteFrame, encoded, &error)
        || encoded != QByteArray("R000001230")) {
        std::cerr << "FAIL: CAN remote frame codec." << std::endl;
        return 9;
    }

    std::cout << "PASS: invalid frames rejected, remote frames and CAN-FD BRS encoded." << std::endl;
    std::cout << "ALL SLCAN FRAME CODEC TESTS PASSED." << std::endl;
    return 0;
}
