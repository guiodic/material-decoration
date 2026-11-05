// SPDX-License-Identifier: GPL-2.0-or-later
#include "AppMenuSearchMatching.h"

#include <QElapsedTimer>
#include <QTest>

namespace Matching = Material::SearchMatching;

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
        // An unstarted timer disables the deadline for score assertions.
        const QElapsedTimer noDeadline;
        QCOMPARE(Matching::calculateFuzzyScore(query, Matching::tokenizeText(query), label,
                                               Matching::tokenizeText(label), noDeadline), expected);
    }
    void respectsSharedDeadline()
    {
        const QString query = QStringLiteral("save");
        const QString label = QStringLiteral("File Save As");
        const auto queryTokens = Matching::tokenizeText(query);
        const auto labelTokens = Matching::tokenizeText(label);
        const QElapsedTimer noDeadline;
        QVERIFY(Matching::calculateFuzzyScore(query, queryTokens, label, labelTokens, noDeadline) > 0);

        QElapsedTimer freshTimer;
        freshTimer.start();
        QVERIFY(Matching::calculateFuzzyScore(query, queryTokens, label, labelTokens, freshTimer) > 0);

        QElapsedTimer timer;
        timer.start();
        QTest::qWait(20);
        // Repeated comparisons must use the original deadline, not restart it.
        QCOMPARE(Matching::calculateFuzzyScore(query, queryTokens, label, labelTokens, timer), 0);
        QCOMPARE(Matching::calculateFuzzyScore(query, queryTokens, label, labelTokens, timer), 0);
    }
    void boundsImpossibleAssignment()
    {
        const QString query = QStringLiteral("orchid quartz velvet zzz zzz zzz");
        const QString label = QStringLiteral("orchid quartz velvet ").repeated(500) + QStringLiteral("zzz zzz");
        const auto queryTokens = Matching::tokenizeText(query);
        const auto labelTokens = Matching::tokenizeText(label);
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(Matching::calculateFuzzyScore(query, queryTokens, label, labelTokens, timer), 0);
        // Generous allowance for instrumented builds and loaded CI machines;
        // the unbounded Release implementation takes several seconds here.
        QVERIFY2(timer.elapsed() < 1000, "Fuzzy scoring did not yield promptly");
    }
};

QTEST_GUILESS_MAIN(AppMenuSearchTest)
#include "AppMenuSearchTest.moc"
