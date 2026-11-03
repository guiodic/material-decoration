// SPDX-License-Identifier: GPL-2.0-or-later
// Include the implementation to exercise the private scoring helpers unchanged.
#include "../src/AppMenuSearch.cc"
#include <QTest>

// Scoring has no model dependency. Supply the only provider method referenced
// by the implementation, plus its Qt metadata slots, so this test does not
// need a live KWin or D-Bus menu. No model instance is constructed.
namespace Material
{
AppMenuModel::~AppMenuModel() = default;
QMenu *AppMenuModel::menu() const { return nullptr; }
void AppMenuModel::onMenuUpdated(QMenu *) {}
void AppMenuModel::onActionChanged() {}
void AppMenuModel::processNext() {}
}

class AppMenuSearchTest : public QObject
{
    Q_OBJECT
private slots:
    void preservesScores_data()
    {
        QTest::addColumn<QString>("query");
        QTest::addColumn<QString>("label");
        QTest::addColumn<int>("expected");
        QTest::newRow("exact") << QStringLiteral("save") << QStringLiteral("File Save As") << 10890;
        QTest::newRow("prefix-tokens") << QStringLiteral("sav as") << QStringLiteral("File Save As") << 2020;
        QTest::newRow("typo") << QStringLiteral("find replce") << QStringLiteral("Edit Find and Replace") << 1730;
        QTest::newRow("phrase") << QStringLiteral("open recent") << QStringLiteral("File Open Recent Documents") << 11590;
        QTest::newRow("no-match") << QStringLiteral("zzzzzz") << QStringLiteral("File Save As") << 0;
    }
    void preservesScores()
    {
        QFETCH(QString, query);
        QFETCH(QString, label);
        QFETCH(int, expected);
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(Material::calculateFuzzyScore(query, Material::tokenizeText(query), label,
                                               Material::tokenizeText(label), timer), expected);
    }
    void respectsSharedDeadline()
    {
        const QString query = QStringLiteral("save");
        const QString label = QStringLiteral("File Save As");
        QElapsedTimer timer;
        timer.start();
        QVERIFY(Material::calculateFuzzyScore(query, Material::tokenizeText(query), label,
                                              Material::tokenizeText(label), timer) > 0);
        QTest::qWait(20);
        // A new comparison must use the original deadline, not restart it.
        QCOMPARE(Material::calculateFuzzyScore(query, Material::tokenizeText(query), label,
                                               Material::tokenizeText(label), timer), 0);
    }
    void boundsImpossibleAssignment()
    {
        const QString query = QStringLiteral("orchid quartz velvet zzz zzz zzz");
        const QString label = QStringLiteral("orchid quartz velvet ").repeated(500) + QStringLiteral("zzz zzz");
        const auto queryTokens = Material::tokenizeText(query);
        const auto labelTokens = Material::tokenizeText(label);
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(Material::calculateFuzzyScore(query, queryTokens, label, labelTokens, timer), 0);
        // Generous allowance for instrumented builds and loaded CI machines;
        // the unbounded Release implementation takes several seconds here.
        QVERIFY2(timer.elapsed() < 1000, "Fuzzy scoring did not yield promptly");
    }
};

QTEST_MAIN(AppMenuSearchTest)
#include "AppMenuSearchTest.moc"
