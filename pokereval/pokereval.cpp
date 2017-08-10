#include "hr_time.h"
#include <array>
#include <fstream>
#include <future>
#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <Synchapi.h>

enum class Suit { Diamonds, Hearts, Clubs, Spades };
enum class Rank { Ace, Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten, Jack, Queen, King};
enum class Value { HighCard, Pair, TwoPair, Three, Straight, Flush, FullHouse, Four, StraightFlush, RoyalFlush  };
static std::string ValueStr[] = { "HighCard", "Pair", "Two Pair", "Three", "Straight", "Flush", "Full House", "Four", "Straight Flush","Royal Flush" };
static std::string SuitStr[]={"D","H","C","S"};
static std::string RankStr[]={"A", "2", "3", "4", "5","6","7", "8", "9", "T", "J", "Q", "K" };
const std::string PokerSuitStrings = "DHCS";
const std::string PokerSuitRanks = "A23456789TJQK";
const int MaxThreads = 12;

class PokerCard {

private:	
	// convert char eg S to spades enum
	static Suit getSuitFromChar(char c) {
		std::size_t found = PokerSuitStrings.find(c); // DHCS
		return static_cast<Suit>(found);
	}

	static Rank getRankFromChar(char c) {
		std::size_t found = PokerSuitRanks.find(c); // A23456789TJQK
		return static_cast<Rank>(found);
	}

public:
	Suit suit;
	Rank rank;
	bool selected; // used for checking for full houses and two pairs

	PokerCard(std::string cardtext) { // RankSuit e.g. 6C = Six Clubs
		rank = getRankFromChar(cardtext[0]);
		suit = getSuitFromChar(cardtext[1]);
	}

	PokerCard() {
		selected = false;
	}

	std::string ToString() { return RankStr[(int)rank] + SuitStr[(int)suit];}; // output 7D for Seven Diamonds
};


class PokerHand {
	std::array<PokerCard, 5> hand; // needs PokerCard default Constructor
	void SortByRank();
	static bool SortByRankComparison(const PokerCard &lhs, const PokerCard &rhs) { return lhs.rank < rhs.rank; }
public:
	PokerHand() {};
	PokerHand(std::string handtext);
	std::string GetResult(Value & handvalue);
	void WriteResult(std::ofstream& stream, Value handvalue);
	friend Value EvaluateHand(PokerHand& Ph);
	std::string ToString();

};

PokerHand::PokerHand(std::string handtext) {
	auto offset = 0;
	auto handoffset = 0;
	for (auto cardIndex = 0; cardIndex < 5; cardIndex++) {
		std::string cardStr(handtext, offset, 2);
		offset += 2;
		PokerCard card(cardStr);
		hand[handoffset++] = card;
	}
	SortByRank();
}


void PokerHand::SortByRank() {
	sort(hand.begin(), hand.end(), SortByRankComparison);
}

// output hand as sorted text
std::string PokerHand::ToString() {
	std::string result = "";
	for (auto card : hand) {
		result += card.ToString(); 
	}
	return result;
}

// Friend function
Value EvaluateHand(PokerHand& Ph) {
	auto isStraight = true;
	auto isFlush = true;
	auto isPair = false;
	auto isThree = false;
	auto isFour = false;
	auto isTwoPair = false;
	auto isFullHouse = false;
	auto sameRank = 0; // count number of cards of same rank

	Sleep(1);
	for (auto& card : Ph.hand) {
		card.selected = false;
	}

	// check flush first
	auto suit = Ph.hand[0].suit;	
	auto isFirst = true;
	for (auto c : Ph.hand) {
		if (isFirst) {
			isFirst = false;
			continue;
		}
		if (c.suit != suit) {
			isFlush = false;
			break; 
		}
	}

	// check straight
	auto rank1 = (int)Ph.hand[0].rank;
	isFirst = true;
	isStraight = true;
	for (auto c : Ph.hand) {
		if (isFirst) { // skip comparison of first card against self
			isFirst = false;
			continue;
		}
		auto rank2 =(int)c.rank;
		if (rank1+1 == rank2 ) // in order or King < Ace
		{
			rank1 = rank2;
			continue;
		}
		isStraight = false;
		break;
	}

	// Check for non straights but with Ace in lowest position, move Ace to last position
	if (!isStraight && Ph.hand[0].rank == Rank::Ace) {
		auto temp = Ph.hand[0];   // Slide cards pos 1-4 into 0-3 then copy previous 0 to 4
		for (auto index = 0; index < 4; index++) { //  So ATJQK becomes TJQKA
			Ph.hand[index] = Ph.hand[index+1];
		}
		Ph.hand[4] = temp;
		rank1 = (int)Ph.hand[0].rank;
		isFirst = true;
		isStraight = true;
		for (auto c : Ph.hand) {
			if (isFirst) { // skip comparison of first card against self
				isFirst = false;
				continue;
			}
			auto rank2 = (int)c.rank;
			if (rank1 + 1 == rank2  || (rank1==12 && rank2 ==0)) // in order or King < Ace
			{
				rank1 = rank2;
				continue;
			}
			isStraight = false;
			break;
		}
	}


	// check for pairs, two pairs, threes, fours, full houses
	auto rank = Ph.hand[0].rank;
	auto hitRank = 0;
	PokerCard * lastCard= &Ph.hand[0];
	sameRank = 1;
	isFirst = true;
	auto hitsomething = false;
	for (auto& c : Ph.hand) {
		c.selected = false;
		if (isFirst) { // skip comparison of first card against self
			isFirst = false;
			lastCard = &c;
			continue;
		}
		if (c.rank == rank) {
			if (sameRank == 1 && hitsomething) break;// only want one pair or trips etc selected
			sameRank++;
			hitsomething = true;
			c.selected = true; // mark cards
			(*lastCard).selected = true; // ensures we select all cards in a pair, trips etc
			hitRank = sameRank;
		}
		else {
			sameRank = 1;
		}
		rank = c.rank;
		lastCard = &c;
	}

	if (hitRank > 0) {

		switch (hitRank) {
		case 2: isPair = true; break;
		case 3: isThree = true; break;
		case 4: isFour = true; break;
		}
		
		// check for pair or trips in non selected
		if (isPair || isThree)
		sameRank = 0;
		isFirst = true;
		for (auto c : Ph.hand) {			
			if (c.selected) continue; // skip previous
			if (isFirst) { // skip comparison of first card against self
				isFirst = false;
				rank = c.rank;
				sameRank = 1;
				continue;
			}

			if (c.rank == rank) {
				sameRank++;
			}
			else {
				sameRank = 1;
			}
			rank = c.rank;
		}
		if (sameRank == 3) { // Full house
			isFullHouse = true;
		}
		else
			if (sameRank == 2) {
				if (isPair) isTwoPair = true;
				else
					if (isThree) isFullHouse = true;
			}		
	}

	if (isStraight && isFlush) {
		if (Ph.hand[4].rank == Rank::Ace)
			return Value::RoyalFlush;
		else
			return Value::StraightFlush;
	}
	if (isFour) return Value::Four;
	if (isFullHouse) return Value::FullHouse;
	if (isFlush) return Value::Flush;
	if (isStraight) return Value::Straight;
	if (isThree) return Value::Three;
	if (isTwoPair) return Value::TwoPair;
	if (isPair) return Value::Pair;
	return Value::HighCard;
}

void PokerHand::WriteResult(std::ofstream& stream,Value handvalue) {
	stream << GetResult( handvalue ) << std::endl;
}

std::string PokerHand::GetResult( Value & handvalue) {
	return ToString() + " " + ValueStr[(int)handvalue];
}

// Only called if uncommented
std::string ProcessThread(std::string str) {
			PokerHand pokerhand(str);			
			auto result = EvaluateHand(pokerhand);
			return pokerhand.GetResult(result);
}
//#define Multi 1
int main()
{
	CStopWatch sw;
	sw.startTimer();
	std::ofstream fileout("results.txt");
	std::ifstream filein("hands.txt");
	std::string str;
	auto rowCount = 0;

#if Multi
	std::array<std::future<std::string>,MaxThreads-1> futures;
	auto count = 0;
	while (count <MaxThreads-1) {
		if (filein.eof()) break;
		std::getline(filein, str);
		rowCount++;
		//futures[count++] = std::async(ProcessThread, str);
		futures[count++] = std::async([str]{
			PokerHand pokerhand(str);
			auto result = EvaluateHand(pokerhand);
			return pokerhand.GetResult(result);
		});
		if (count == MaxThreads-1) {
			for (auto & e : futures) {
				fileout << e.get() << std::endl;
			}
			count = 0;
		}
	}
	if (count != MaxThreads - 1) {
		for (int i = 0; i < count; ++i) {
			fileout << futures[i].get() << std::endl;
		}
	}
#else
	while (std::getline(filein, str))
	{
		PokerHand pokerhand(str);
		auto result = EvaluateHand(pokerhand);
		pokerhand.WriteResult(fileout, result);
		rowCount++;
	}

	#endif	fileout.close();
	filein.close();
	sw.stopTimer();
	std::cout << "Time to evaluate " << rowCount << " poker hands: " << sw.getElapsedTime() << std::endl;
	return 0;
}
