// Standalone smoke test for the Eagle-style metadata / search layer.
// Builds materiallibrary.cpp directly (no CMake) and exercises:
//   - importFromImage (clipboard paste path)
//   - setRating / setTags / addTag / setNote / setSource + round-trip through save/load
//   - structured search syntax: tag:, rating:>=N, ext:, name:, bare word, multi-token AND
//   - allTags() aggregation
//
// Exit code 0 = all assertions passed, non-zero = first failure count.
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QImageWriter>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <memory>

// Pull the implementation in via separate .cpp translation units (see build cmd).
#include "../PureRefCanvas/materiallibrary.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else         { fprintf(stdout, "ok  : %s\n", msg); } \
    } while (0)

int main(int argc, char **argv)
{
    // QGuiApplication (not QCoreApplication): PNG/JPEG image format plugins
    // only load under a GUI app, and importFromImage() relies on QImage::save.
    QGuiApplication app(argc, argv);

    // Fresh temp library root — put it next to the exe so we don't depend on
    // system temp permissions.
    const QString root = QDir::currentPath() + "/sandbox_lib";
    QDir(root).removeRecursively();
    QDir().mkpath(root);

    {
        MaterialLibrary lib(root);

        // 1. import a 12x8 red image as if pasted from clipboard
        QImage img(12, 8, QImage::Format_RGB32);
        img.fill(qRgb(220, 30, 30));
        const QString id = lib.importFromImage(img, "icons", "https://example.com/pic.png", "https://example.com/page.html");
        CHECK(!id.isEmpty(), "importFromImage returns id");
        MaterialItem it = lib.itemById(id);
        CHECK(it.id == id, "itemById round-trip");
        CHECK(it.width == 12 && it.height == 8, "image dimensions recorded");
        CHECK(it.type == AssetImage, "type detected as image");
        CHECK(it.sourceUrl == "https://example.com/pic.png", "sourceUrl recorded");
        CHECK(it.sourceReferrer == "https://example.com/page.html", "referrer recorded");

        // 2. Mutate metadata
        lib.setRating(id, 4);
        lib.addTag(id, "icon");
        lib.addTag(id, "Red");
        lib.addTag(id, "icon"); // duplicate — must be deduped
        lib.setNote(id, "main app mark");

        // 3. Import a second item: a 800x600 video-like .mp4 (empty file, type detection by ext)
        QString fakeVideo = QDir::currentPath() + "/sample.mp4";
        { QFile f(fakeVideo); if (f.open(QIODevice::WriteOnly)) f.write("x"); }
        lib.importFiles({fakeVideo}, QString(), "https://videos.example.com/a.mp4", "");
        QFile::remove(fakeVideo);
        CHECK(lib.allItems().size() == 2, "two items in library");
    }

    // 4. Re-open the library from disk — metadata must survive the round-trip.
    {
        MaterialLibrary lib(root);
        CHECK(lib.allItems().size() == 2, "load() restores 2 items");

        MaterialItem png;
        for (const MaterialItem &it : lib.allItems())
            if (it.fileName.endsWith(".png")) png = it;
        CHECK(!png.id.isEmpty(), "png item found after reload");
        CHECK(png.rating == 4, "rating persisted across save/load");
        CHECK(png.tags.size() == 2, "tags deduped (icon, Red)");
        CHECK(png.note == "main app mark", "note persisted");
        CHECK(png.sourceUrl == "https://example.com/pic.png", "sourceUrl persisted");

        // 5. Structured search
        CHECK(lib.searchItems("").size() == 2, "empty query returns all");
        CHECK(lib.searchItems("icon").size() == 1, "bare word matches tag");
        CHECK(lib.searchItems("tag:icon").size() == 1, "tag:icon hits");
        CHECK(lib.searchItems("tag:ICON").size() == 1, "tag: is case-insensitive");
        CHECK(lib.searchItems("tag:nope").size() == 0, "tag: missing -> empty");
        CHECK(lib.searchItems("rating:4").size() == 1, "rating:4 exact");
        CHECK(lib.searchItems("rating:>=4").size() == 1, "rating:>=4");
        CHECK(lib.searchItems("rating:>=5").size() == 0, "rating:>=5 misses");
        CHECK(lib.searchItems("rating:<4").size() == 1, "rating:<4 hits the unrated video");
        CHECK(lib.searchItems("ext:png").size() == 1, "ext:png");
        CHECK(lib.searchItems("ext:mp4").size() == 1, "ext:mp4");
        CHECK(lib.searchItems("name:pasted").size() == 1, "name: matches baseName");
        CHECK(lib.searchItems("note:main").size() == 1, "note: matches note");
        CHECK(lib.searchItems("tag:icon ext:png").size() == 1, "multi-token AND");
        CHECK(lib.searchItems("tag:icon ext:mp4").size() == 0, "contradictory AND = empty");
        CHECK(lib.searchItems("example.com").size() == 2, "bare word matches sourceUrl (both items)");

        // 6. allTags() aggregation
        QStringList tags = lib.allTags();
        CHECK(tags.contains("icon") && tags.contains("Red"), "allTags() lists both tags");

        // 7. History table: every metadata mutation should have been logged.
        {
            QSqlDatabase hdb = QSqlDatabase::addDatabase("QSQLITE", "hist_check");
            hdb.setDatabaseName(root + "/library.db");
            CHECK(hdb.open(), "history db opens");
            QSqlQuery q(hdb);
            q.exec("SELECT COUNT(*) FROM history");
            CHECK(q.next() && q.value(0).toInt() >= 3, "history has >=3 rows");
            q.exec("SELECT field, old_value, new_value FROM history WHERE field='rating' ORDER BY id DESC LIMIT 1");
            CHECK(q.next() && q.value(0).toString() == "rating", "rating history row recorded");
            hdb.close();
            QSqlDatabase::removeDatabase("hist_check");
        }
    }

    QDir(root).removeRecursively();

    if (failures == 0) fprintf(stdout, "ALL TESTS PASSED\n");
    else               fprintf(stderr, "%d check(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
