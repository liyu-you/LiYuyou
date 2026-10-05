// Standalone console check for reading-time accounting (no QApplication).
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cstdio>
#include "booklibrary.h"

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const QString root = QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/scratch_accounting");
    // Wipe for a deterministic, isolated run.
    QDir d(root);
    d.removeRecursively();
    QDir().mkpath(root);
    const QString bookPath = root + "/_probe.txt";
    QFile f(bookPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { std::printf("FAIL write\n"); return 1; }
    QTextStream ts(&f); ts << "第一章\n正文内容一二三四五六七八九十。\n"; f.close();

    BookLibrary lib(root);
    if (!lib.load()) { std::printf("FAIL load\n"); return 1; }
    QString id = lib.importBook(bookPath);
    if (id.isEmpty()) { std::printf("FAIL import\n"); return 1; }
    std::printf("imported id=%s\n", id.toUtf8().constData());

    lib.addReadingSeconds(id, 120);
    std::printf("today=%lld all=%lld\n", (long long)lib.totalTodayReadSec(), (long long)lib.totalAllReadSec());
    if (lib.totalTodayReadSec() != 120 || lib.totalAllReadSec() != 120) { std::printf("FAIL accumulate\n"); return 1; }
    if (!lib.save()) { std::printf("FAIL save\n"); return 1; }

    BookLibrary lib2(root);
    if (!lib2.load()) { std::printf("FAIL reload\n"); return 1; }
    std::printf("after reload today=%lld all=%lld\n", (long long)lib2.totalTodayReadSec(), (long long)lib2.totalAllReadSec());
    if (lib2.totalTodayReadSec() != 120 || lib2.totalAllReadSec() != 120) { std::printf("FAIL persist\n"); return 1; }

    std::printf("PASS accounting\n");
    return 0;
}
