#pragma once
#include <QByteArray>
#include <QPair>

namespace reposco::testing {
inline QByteArray htmlClipboard(const QByteArray& fragment) {
    QByteArray header("Version:1.0\r\nStartHTML:0000000000\r\nEndHTML:0000000000\r\n"
                      "StartFragment:0000000000\r\nEndFragment:0000000000\r\n");
    const QByteArray start("<html><body><!--StartFragment-->");
    const QByteArray end("<!--EndFragment--></body></html>");
    const auto startHtml = header.size();
    const auto startFragment = startHtml + start.size();
    const auto endFragment = startFragment + fragment.size();
    const auto endHtml = endFragment + end.size();
    for (const auto& pair : {qMakePair(QByteArray("StartHTML:"), startHtml),
                             qMakePair(QByteArray("EndHTML:"), endHtml),
                             qMakePair(QByteArray("StartFragment:"), startFragment),
                             qMakePair(QByteArray("EndFragment:"), endFragment)}) {
        header.replace(header.indexOf(pair.first) + pair.first.size(), 10,
                       QByteArray::number(pair.second).rightJustified(10, '0'));
    }
    return header + start + fragment + end;
}
} // namespace reposco::testing
