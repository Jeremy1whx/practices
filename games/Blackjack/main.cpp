#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <string>
#include <random> 
#include <thread>
#include <chrono>

struct Card {
    std::string rank;
    int value;
};

class BlackjackGame {
private:
    std::vector<Card> deck;
    std::vector<Card> playerHand;
    std::vector<Card> dealerHand;
    std::vector<std::vector<Card>> splitHands;  // Store multiple hands after split
    std::mt19937 rng;
    int playerMoney;
    int currentBet;
    bool hasSplit;

    // Initialize a standard deck of 52 cards
    void initDeck() {
        deck.clear();
        std::string ranks[] = {"2","3","4","5","6","7","8","9","10","J","Q","K","A"};
        int values[] = {2,3,4,5,6,7,8,9,10,10,10,10,11};

        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 13; ++j) {
                Card c;
                c.rank = ranks[j];
                c.value = values[j];
                deck.push_back(c);
            }
        }
    }

    // Shuffle the deck randomly
    void shuffleDeck() {
        shuffle(deck.begin(), deck.end(), rng);
    }

    // Draw a card from the top of the deck
    Card drawCard() {
        Card c = deck.back();
        deck.pop_back();
        return c;
    }

    // Calculate the best hand value (handles Ace as 1 or 11)
    int calcHandValue(const std::vector<Card>& hand) {
        int sum = 0;
        int aceCount = 0;

        for (const auto& card : hand) {
            sum += card.value;
            if (card.rank == "A") {
                aceCount++;
            }
        }

        // Convert Aces from 11 to 1 if bust
        while (sum > 21 && aceCount > 0) {
            sum -= 10;
            aceCount--;
        }
        return sum;
    }

    // Display the hand (can hide dealer's first card)
    void showHand(const std::vector<Card>& hand, bool isDealerFirstHidden = false) {
        if (isDealerFirstHidden) {
            std::cout << "[ Hidden ] ";
            for (size_t i = 1; i < hand.size(); ++i) {
                std::cout << hand[i].rank << " ";
            }
            std::cout << "\n";
        } else {
            for (const auto& card : hand) {
                std::cout << card.rank << " ";
            }
            std::cout << "\n";
        }
    }

    // Display the score of a hand
    void showScore(const std::vector<Card>& hand, const std::string& owner) {
        std::cout << owner << " points: " << calcHandValue(hand) << std::endl;
    }

    // Check if split is possible (two identical cards, not already split)
    bool canSplit() {
        return !hasSplit && playerHand.size() == 2 && 
               playerHand[0].rank == playerHand[1].rank;
    }

    // Check if double down is possible (only two cards in hand)
    bool canDoubleDown() {
        return playerHand.size() == 2;
    }

    // Handle the split operation
    void processSplit() {
        hasSplit = true;
        splitHands.clear();
        
        // Create two hands from the split
        std::vector<Card> hand1, hand2;
        hand1.push_back(playerHand[0]);
        hand2.push_back(playerHand[1]);
        
        // Deal one more card to each hand
        hand1.push_back(drawCard());
        hand2.push_back(drawCard());
        
        splitHands.push_back(hand1);
        splitHands.push_back(hand2);
        
        std::cout << "\n=== Split! You now have 2 hands ===\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Process each hand separately
        int totalWinnings = 0;
        for (size_t i = 0; i < splitHands.size(); ++i) {
            std::cout << "\n--- Playing Hand " << (i + 1) << " ---\n";
            std::cout << "Hand " << (i + 1) << ": ";
            showHand(splitHands[i], false);
            showScore(splitHands[i], "Hand");
            
            bool handBusted = false;
            while (true) {
                if (calcHandValue(splitHands[i]) == 21) {
                    std::cout << "Blackjack on Hand " << (i + 1) << "!\n";
                    break;
                }
                
                std::cout << "Hit (h), Stand (s), or Double Down (d): ";
                char action;
                std::cin >> action;
                
                if (action == 'h') {
                    Card newCard = drawCard();
                    splitHands[i].push_back(newCard);
                    std::cout << "You drew: " << newCard.rank << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    
                    if (calcHandValue(splitHands[i]) > 21) {
                        std::cout << "Hand " << (i + 1) << " busted!\n";
                        handBusted = true;
                        break;
                    }
                    
                    std::cout << "Hand " << (i + 1) << " now: ";
                    showHand(splitHands[i], false);
                    showScore(splitHands[i], "Hand");
                } else if (action == 's') {
                    break;
                } else if (action == 'd' && splitHands[i].size() == 2) {
                    // Double down after split
                    if (playerMoney >= currentBet) {
                        playerMoney -= currentBet;
                        currentBet *= 2;
                        std::cout << "Double Down! Bet increased to $" << currentBet << std::endl;
                        
                        Card newCard = drawCard();
                        splitHands[i].push_back(newCard);
                        std::cout << "You drew: " << newCard.rank << std::endl;
                        showScore(splitHands[i], "Hand");
                        break;
                    } else {
                        std::cout << "Insufficient funds to double down!\n";
                    }
                } else {
                    std::cout << "Invalid input. Use h, s, or d\n";
                }
            }
            
            // Compare with dealer and calculate winnings
            if (!handBusted) {
                int playerScore = calcHandValue(splitHands[i]);
                int dealerScore = calcHandValue(dealerHand);
                
                std::cout << "\nDealer's hand: ";
                showHand(dealerHand, false);
                showScore(dealerHand, "Dealer");
                
                if (playerScore > dealerScore && dealerScore <= 21) {
                    std::cout << "Hand " << (i + 1) << " wins! +$" << currentBet << std::endl;
                    totalWinnings += currentBet * 2;
                } else if (playerScore == dealerScore) {
                    std::cout << "Hand " << (i + 1) << " pushes! +$" << currentBet << std::endl;
                    totalWinnings += currentBet;
                } else if (dealerScore > 21) {
                    std::cout << "Dealer busted! Hand " << (i + 1) << " wins! +$" << currentBet << std::endl;
                    totalWinnings += currentBet * 2;
                } else {
                    std::cout << "Hand " << (i + 1) << " loses! -$" << currentBet << std::endl;
                }
            } else {
                std::cout << "Hand " << (i + 1) << " busted! -$" << currentBet << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        playerMoney += totalWinnings;
        std::cout << "\n=== Split Complete ===\n";
        std::cout << "Total winnings from split: $" << (totalWinnings - currentBet * 2) << std::endl;
        std::cout << "Current money: $" << playerMoney << std::endl;
    }

public:
    BlackjackGame() : rng(std::random_device{}()), playerMoney(1000), hasSplit(false) {}

    // Main game loop
    void play() {
        char choice;
        do {
            initDeck();
            shuffleDeck();
            playerHand.clear();
            dealerHand.clear();
            splitHands.clear();
            hasSplit = false;
            
            // Place bet
            std::cout << "\n========================================\n";
            std::cout << "Current money: $" << playerMoney << std::endl;
            std::cout << "Enter your bet (minimum $10): $";
            std::cin >> currentBet;
            
            while (currentBet < 10 || currentBet > playerMoney) {
                if (currentBet < 10) {
                    std::cout << "Minimum bet is $10. Enter bet: $";
                } else {
                    std::cout << "Insufficient funds! You have $" << playerMoney << ". Enter bet: $";
                }
                std::cin >> currentBet;
            }
            
            playerMoney -= currentBet;
            std::cout << "Bet placed: $" << currentBet << std::endl;
            std::cout << "========================================\n";
            
            // Deal initial cards
            playerHand.push_back(drawCard());
            dealerHand.push_back(drawCard());
            playerHand.push_back(drawCard());
            dealerHand.push_back(drawCard());

            // Check if dealer has Blackjack
            if (calcHandValue(dealerHand) == 21) {
                std::cout << "\nDealer has Blackjack!\n";
                if (calcHandValue(playerHand) == 21) {
                    std::cout << "You also have Blackjack! Push!\n";
                    playerMoney += currentBet;  // Return bet
                } else {
                    std::cout << "You lose!\n";
                }
                
                std::cout << "\nPlay again? (y/n): ";
                std::cin >> choice;
                continue;
            }

            bool playerBusted = false;
            bool playerBlackjack = false;
            
            // Player's turn
            while (true) {
                std::cout << "\n========== Player's Turn ==========\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::cout << "Dealer's visible card: ";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                showHand(dealerHand, true);
                std::cout << "Your hand: ";
                showHand(playerHand, false);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                showScore(playerHand, "Player");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                int playerValue = calcHandValue(playerHand);
                if (playerValue == 21) {
                    std::cout << "Congratulations! You got Blackjack!\n";
                    playerBlackjack = true;
                    break;
                }

                // Display available options
                std::cout << "\nOptions:\n";
                std::cout << "  (h) Hit\n";
                std::cout << "  (s) Stand\n";
                if (canDoubleDown()) {
                    std::cout << "  (d) Double Down (bet: $" << currentBet << ")\n";
                }
                if (canSplit()) {
                    std::cout << "  (p) Split (requires another $" << currentBet << ")\n";
                }
                std::cout << "Choose: ";
                
                char action;
                std::cin >> action;
                
                if (action == 'h') {
                    Card newCard = drawCard();
                    playerHand.push_back(newCard);
                    std::cout << "You drew: " << newCard.rank << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    if (calcHandValue(playerHand) > 21) {
                        std::cout << "\nPlayer busted! Points: " << calcHandValue(playerHand) << std::endl;
                        playerBusted = true;
                        break;
                    }
                } else if (action == 's') {
                    break;
                } else if (action == 'd' && canDoubleDown()) {
                    // Double down functionality
                    if (playerMoney >= currentBet) {
                        playerMoney -= currentBet;
                        currentBet *= 2;
                        std::cout << "\nDouble Down! Bet increased to $" << currentBet << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        
                        Card newCard = drawCard();
                        playerHand.push_back(newCard);
                        std::cout << "You drew: " << newCard.rank << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        showScore(playerHand, "Player");
                        
                        if (calcHandValue(playerHand) > 21) {
                            std::cout << "Player busted!\n";
                            playerBusted = true;
                        }
                        break;  // Double down ends the turn
                    } else {
                        std::cout << "Insufficient funds to double down!\n";
                    }
                } else if (action == 'p' && canSplit()) {
                    // Split functionality
                    if (playerMoney >= currentBet) {
                        playerMoney -= currentBet;
                        std::cout << "\n=== Splitting hand ===\n";
                        processSplit();
                        playerBusted = true;  // Split already handled, skip dealer turn
                        break;
                    } else {
                        std::cout << "Insufficient funds to split!\n";
                    }
                } else {
                    std::cout << "Invalid input!\n";
                }
            }

            // Dealer's turn (only if player didn't bust and didn't split)
            if (!playerBusted && !hasSplit) {
                std::cout << "\n========== Dealer's Turn ==========\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::cout << "Dealer's hand: ";
                showHand(dealerHand, false);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                showScore(dealerHand, "Dealer");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                // Dealer must hit on 16 or less, stand on 17 or more
                while (calcHandValue(dealerHand) < 17) {
                    Card newCard = drawCard();
                    dealerHand.push_back(newCard);
                    std::cout << "Dealer hits, drew: " << newCard.rank << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    std::cout << "Dealer's hand: ";
                    showHand(dealerHand, false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    showScore(dealerHand, "Dealer");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                // Settlement
                int playerScore = calcHandValue(playerHand);
                int dealerScore = calcHandValue(dealerHand);

                std::cout << "\n========== Final Result ==========\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                std::cout << "Player points: " << playerScore << std::endl;
                std::cout << "Dealer points: " << dealerScore << std::endl;

                if (dealerScore > 21) {
                    std::cout << "\nDealer busted! Player wins!\n";
                    playerMoney += currentBet * 2;
                    std::cout << "You won $" << currentBet * 2 << "! ";
                } else if (playerScore > dealerScore) {
                    std::cout << "\nPlayer wins!\n";
                    playerMoney += currentBet * 2;
                    std::cout << "You won $" << currentBet * 2 << "! ";
                } else if (playerScore < dealerScore) {
                    std::cout << "\nDealer wins!\n";
                    std::cout << "You lost $" << currentBet << ". ";
                } else {
                    std::cout << "\nTie! Push.\n";
                    playerMoney += currentBet;
                    std::cout << "Bet returned: $" << currentBet << ". ";
                }
                
                // Blackjack bonus (pays 3:2)
                if (playerBlackjack && playerScore == 21 && dealerScore != 21) {
                    int blackjackBonus = currentBet / 2;
                    playerMoney += blackjackBonus;
                    std::cout << "Blackjack bonus: +$" << blackjackBonus << "! ";
                }
                
                std::cout << "Current money: $" << playerMoney << std::endl;
            } else if (!playerBusted && hasSplit) {
                // Split already handled settlement in processSplit()
                std::cout << std::endl;
            }

            // Game over if player runs out of money
            if (playerMoney <= 0) {
                std::cout << "\n========================================\n";
                std::cout << "You're out of money! Game over!\n";
                std::cout << "========================================\n";
                break;
            }

            std::cout << "\nPlay again? (y/n): ";
            std::cin >> choice;
        } while (choice == 'y' || choice == 'Y');

        std::cout << "\nThanks for playing!\n";
        std::cout << "Final money: $" << playerMoney << std::endl;
    }
};

int main() {
    BlackjackGame game;
    game.play();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}