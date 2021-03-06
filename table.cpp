#include "table.h"
#include "player.h"
#include "preflopplayer.h"

#define MAX_BETTINGROUNDS 5

#include <QDebug>

Table::Table(Phase phase) :
        m_pot(0),
        m_lastBet(0),
        m_phase(phase)
{
    switch (phase) {
    case (III):
        qFatal("NOT IMPLEMENTED");
    case (II):
        m_players << RolloutPlayer(false)
                  << RolloutPlayer(false)
                  << RolloutPlayer(true)
                  << RolloutPlayer(true);
    case (I):
        m_players << Player(true)
                  << Player(false)
                  << Player(true)
                  << Player(false);
    }
}

void Table::play(int rounds)
{
    if (m_phase == I) {
        qDebug() << "###########";
        qDebug() << "# Phase 1 #";
        qDebug() << "###########";
    } else if (m_phase == II) {
        qDebug() << "###########";
        qDebug() << "# Phase 2 #";
        qDebug() << "###########";
    }

    for (int round=0; round<rounds; round++) {
        playHand();
    }

    qDebug() << "-------------------";
    qDebug() << "Finished playing...";
    for (int i=0; i<m_players.size(); i++) {
        qWarning() << m_players[i].name() << "won"
                << m_players[i].wins()
                << "times and tied"
                << m_players[i].ties() << "times. It has"
                << m_players[i].money() << "casharoos.";
    }
}

void Table::playHand()
{
    m_players.append(m_players.takeFirst()); //rotate players playing first

    qDebug() << "Initiating hand.";
    m_lastBet = 0;
    m_flop.clear();
    m_turn = Card(); // null card
    m_river = Card();

    Deck deck;
    deck.generate();
    deck.shuffle();

    // Blinds
    m_pot = 150;
    m_players[0].takeMoney(50); // little blind
    m_players[1].takeMoney(100); // big blind
    m_lastBet = 100;

    for (int i=0; i<m_players.size(); i++) {
        qDebug() << "Giving player" << m_players[i].name() << "a deck...";
        m_players[i].setDeck(deck.take(2));
        qDebug() << "Player" << m_players[i].name() << "has" << m_players[i].money() << "monies.";
    }


    // Initial betting round
    doBettingRound();

    // Flop
    qDebug() << "Burn card before flop:" << deck.takeLast().toString(); // burn card
    m_flop = deck.take(3);
    qDebug() << "Flop:";
    m_flop.printOut();
    if (activePlayers() > 1)
        doBettingRound();

    // Turn
    qDebug() << "Burn card before turn:" << deck.takeLast().toString(); // burn card
    m_turn = deck.takeLast();
    qDebug() << "Turn card:" << m_turn.toString();
    if (activePlayers() > 1)
        doBettingRound();

    // River
    qDebug() << "Burn card before river:" << deck.takeLast().toString(); // burn card
    m_river = deck.takeLast();
    qDebug() << "River card:" << m_river.toString();
    if (activePlayers() > 1)
        doBettingRound();

    // Check if all but one has folded
    if (activePlayers() < 2) {
        for (int i=0; i<m_players.size(); i++) {
            if (!m_players[i].hasFolded()) {
                qDebug() << "Player" << m_players[i].name() << "has won" << m_pot << "monies.";
                m_players[i].win();
                m_players[i].giveMoney(m_pot);
                return;
            }
        }
    }

/*    QList<int> winners;
    for (int i=0; i<m_players.size(); i++)
        winners << i;
    for (int i=0; i<m_players.size(); i++)
        if (m_players[i].hasFolded())
            winners.removeAll(i);*/

    Deck winner, hand;
    QList<int> winners;
    // Stupid, late, slow, not thinking
    for (int i=0; i<m_players.size(); i++) {
        if (m_players[i].hasFolded()) continue;

        hand = m_players[i].deck();
        hand << m_flop
             << m_river
             << m_turn;
        if (winner.isEmpty()) {
            winner = hand;
            winners << i;
            continue;
        }

        if (winner < hand) {
            winners.clear();
            winners << i;
            winner = hand;
        } else if (!(hand < winner)) {
            winners << i;
        }
    }

    qDebug() << "----------------------------------\nHand over.";
    qDebug() << "We have" << winners.size() << "winners.";

    foreach(const Player &player, m_players) {
        if (player.hasFolded())
            qDebug() << player.name() << "has folded.";
        else {
            qDebug() << player.name() << "has is in showdown:";
            player.deck().printOut();
            qDebug() << "---";
        }
    }
    qDebug() << "Flop:";
    m_flop.printOut();
    qDebug() << "Turn:" << m_turn.toString();
    qDebug() << "River:" << m_river.toString();

    if (winners.size() > 1) {
        int potPayout = m_pot / winners.size();
        foreach(int i, winners) {
            m_players[i].tie();
            qDebug() << "Player" << m_players[i].name() << "has tied and gotten" << potPayout << "casharoos.";
            m_players[i].giveMoney(potPayout);
        }
    }
    else {
        qDebug() << "Player" << m_players[winners[0]].name() << "has won" << m_pot;
        m_players[winners[0]].win();
        m_players[winners[0]].giveMoney(m_pot);
    }
}

void Table::doBettingRound()
{
    Player::Action action;

    for (int i=0; i<m_players.size(); i++) {
        m_players[i].bets = 0;
    }

    int called = 0, current = 0, players = activePlayers();
    do {
        current++;
        current %= m_players.size();

        if (!m_players[current].hasFolded()) {
            action = m_players[current].assess(this);
            m_players[current].bets++;
            if (m_players[current].bets > MAX_BETTINGROUNDS)
                action = Player::Call;

            switch(action) {
            case (Player::Fold):
                qDebug() << "Player" << m_players[current].name() << "folded.";
                called++;
                break;
            case (Player::Call):
                qDebug() << "Player" << m_players[current].name() << "called.";
                m_pot += m_lastBet;
                m_players[current].takeMoney(m_lastBet);
                called++;
                break;
            case (Player::Raise):
                qDebug() << "Player" << m_players[current].name() << "raised to" << m_players[current].bet() << ".";
                m_pot += m_players[current].bet();
                m_lastBet = m_players[current].bet();
                called = 0; // Everyone now needs to call or fold to this raise
                m_players[current].takeMoney(m_players[current].bet());
            }

            Deck community = m_flop;
            community << m_turn << m_river;
            m_models[m_players[current].name()].addContext(lastBet() / (float)(lastBet() + pot()), action, community, m_players[current].handStrength(this));
        }
        //qDebug() << "Active players:" << activePlayers();
        if (activePlayers() < 2)
            return;
    } while (called < players); // make sure all people have called (or folded) since last raise
}

int Table::activePlayers()
{
    int c=0;
    for (int i=0; i<m_players.size(); i++) {
        if (!m_players[i].hasFolded())
            c++;
    }
    return c;
}
