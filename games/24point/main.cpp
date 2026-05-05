#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <sstream>
#include <stack>
#include <cmath>
#include <optional>
#include <memory>
#include <array>
#include <concepts>
#include <functional>



// Type aliases for better readability
using Card = int;
using CardSet = std::vector<Card>;
using Expression = std::string;

// Constants
constexpr double TARGET_VALUE = 24.0;
constexpr double EPSILON = 1e-6;
constexpr int CARDS_PER_GAME = 4;
constexpr int MIN_CARD_VALUE = 1;
constexpr int MAX_CARD_VALUE = 13;

/**
 * Converts numeric card value to display character
 * @param value Card value (1-13)
 * @return Display string (A, 2-10, J, Q, K)
 */
std::string cardToChar(Card value) noexcept {
    switch (value) {
        case 1:  return "A";
        case 11: return "J";
        case 12: return "Q";
        case 13: return "K";
        default: return std::to_string(value);
    }
}

/**
 * Random number generator using C++20 random facilities
 */
class RandomGenerator {
private:
    std::mt19937 rng;
    
public:
    RandomGenerator() {
        auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        rng = std::mt19937(static_cast<unsigned int>(seed));
    }
    
    int nextInt(int min, int max) noexcept {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }
};

// Global random generator instance
inline RandomGenerator g_rng;

/**
 * Generates 4 random cards (values 1-13)
 * @return Vector of 4 random card values
 */
CardSet generateCards() noexcept {
    CardSet cards(CARDS_PER_GAME);
    for (auto& card : cards) {
        card = g_rng.nextInt(MIN_CARD_VALUE, MAX_CARD_VALUE);
    }
    return cards;
}

/**
 * Displays the current cards to the console
 * @param cards Vector of card values to display
 */
void displayCards(const CardSet& cards) noexcept {
    std::cout << "Current cards: ";
    for (const auto& card : cards) {
        std::cout << cardToChar(card) << " ";
    }
    std::cout << std::endl;
}

/**
 * Tokenizes and evaluates a mathematical expression
 * Supports +, -, *, / and parentheses
 * Uses shunting-yard algorithm with two stacks
 * 
 * @param expr Mathematical expression string
 * @return Evaluated result (or NaN if error)
 */
double evaluateExpression(const std::string& expr) noexcept {
    std::stack<double> values;
    std::stack<char> operators;
    
    // Lambda for applying an operator
    auto applyOperator = [&]() -> void {
        if (values.size() < 2 || operators.empty()) return;
        
        double b = values.top(); values.pop();
        double a = values.top(); values.pop();
        char op = operators.top(); operators.pop();
        
        double result = 0.0;
        switch (op) {
            case '+': result = a + b; break;
            case '-': result = a - b; break;
            case '*': result = a * b; break;
            case '/': 
                if (fabs(b) < EPSILON) {
                    result = nan("");  // Division by zero
                } else {
                    result = a / b;
                }
                break;
            default: result = nan("");
        }
        values.push(result);
    };
    
    // Operator precedence
    auto precedence = [](char op) -> int {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/') return 2;
        return 0;
    };
    
    // Parse the expression
    for (size_t i = 0; i < expr.length(); ++i) {
        char c = expr[i];
        if (isspace(c)) continue;
        
        if (isdigit(c)) {
            // Parse multi-digit numbers
            double num = 0;
            while (i < expr.length() && isdigit(expr[i])) {
                num = num * 10 + (expr[i] - '0');
                ++i;
            }
            --i;
            values.push(num);
        }
        else if (c == '(') {
            operators.push(c);
        }
        else if (c == ')') {
            while (!operators.empty() && operators.top() != '(') {
                applyOperator();
            }
            if (!operators.empty()) operators.pop();
        }
        else if (c == '+' || c == '-' || c == '*' || c == '/') {
            while (!operators.empty() && operators.top() != '(' && 
                   precedence(operators.top()) >= precedence(c)) {
                applyOperator();
            }
            operators.push(c);
        }
    }
    
    // Apply remaining operators
    while (!operators.empty()) {
        applyOperator();
    }
    
    if (values.empty()) return nan("");
    double result = values.top();
    
    // Round to handle floating-point precision issues
    if (fabs(result - round(result)) < EPSILON) {
        result = round(result);
    }
    
    return result;
}

/**
 * Checks if the expression uses exactly the given cards
 * Parses numbers from expression and validates against card set
 * 
 * @param expr User's expression
 * @param cards Required card set
 * @return true if each card is used exactly once
 */
bool validateCardUsage(const std::string& expr, const CardSet& cards) noexcept {
    std::vector<int> usedNumbers;
    std::stringstream ss(expr);
    char ch;
    
    // Parse all numbers from expression
    while (ss >> ch) {
        if (isdigit(ch)) {
            int num = ch - '0';
            // Handle multi-digit numbers (10-13)
            if (ss.peek() >= '0' && ss.peek() <= '9') {
                int next = ss.peek() - '0';
                if (num == 1 && next <= 3) {
                    num = 10 + next;
                    ss.get();                    
                }
            }
            usedNumbers.push_back(num);
        }
    }
    
    // Must use exactly 4 numbers
    if (usedNumbers.size() != CARDS_PER_GAME) return false;
    
    // Check if numbers match the cards (each used exactly once)
    CardSet remainingCards = cards;
    for (int num : usedNumbers) {
        auto it = find(remainingCards.begin(), remainingCards.end(), num);
        if (it == remainingCards.end()) return false;
        remainingCards.erase(it);
    }
    
    return true;
}

// Check if the expression uses correct parentheses
bool validateParentheses(const std::string& expr) {
    std::stack<char> parentheses;
    
    for (char c : expr) {
        if (c == '(') {
            parentheses.push(c);
        }
        else if (c == ')') {
            if (parentheses.empty()) {
                std::cout << "Error: Unmatched closing parenthesis ')'" << std::endl;
                return false;
            }
            parentheses.pop();
        }
        else if (c == '+' || c == '-' || c == '*' || c == '/' || isdigit(c)) {continue;}
        else {
            std::cout << "Error: Invalid input" << std::endl;
            return false;
        }
    }
    
    if (!parentheses.empty()) {
        std::cout << "Error: Unmatched opening parenthesis '('" << std::endl;
        return false;
    }
    
    return true;
}

/**
 * Structure representing a solution to the 24-point puzzle
 */
struct Solution {
    bool hasSolution;
    std::string expression;
    
    Solution() : hasSolution(false), expression("") {}
    Solution(bool sol, std::string expr) : hasSolution(sol), expression(std::move(expr)) {}
};

/**
 * Recursively solves the 24-point game
 * Uses backtracking to try all possible combinations of operations
 * 
 * @param numbers Current list of numbers
 * @param expressions Corresponding expression strings
 * @param result Reference to store the found solution
 * @return true if a solution is found
 */
bool solveRecursive(std::vector<double>& numbers, std::vector<std::string>& expressions, std::string& result) {
    // Base case: only one number left
    if (numbers.size() == 1) {
        if (fabs(numbers[0] - TARGET_VALUE) < EPSILON) {
            result = expressions[0];
            return true;
        }
        return false;
    }
    
    const int n = numbers.size();
    
    // Try all pairs of numbers
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double a = numbers[i];
            double b = numbers[j];
            std::string exprA = expressions[i];
            std::string exprB = expressions[j];
            
            // Create new vectors without the selected pair
            std::vector<double> newNumbers;
            std::vector<std::string> newExpressions;
            
            for (int k = 0; k < n; ++k) {
                if (k != i && k != j) {
                    newNumbers.push_back(numbers[k]);
                    newExpressions.push_back(expressions[k]);
                }
            }
            
            // Try all arithmetic operations
            struct Operation {
                double value;
                std::string expression;
            };
            std::vector<Operation> operations;
            
            // Addition
            operations.push_back({a + b, "(" + exprA + "+" + exprB + ")"});
            
            // Subtraction (both orders)
            operations.push_back({a - b, "(" + exprA + "-" + exprB + ")"});
            operations.push_back({b - a, "(" + exprB + "-" + exprA + ")"});
            
            // Multiplication
            operations.push_back({a * b, "(" + exprA + "*" + exprB + ")"});
            
            // Division (avoid division by zero)
            if (fabs(b) > EPSILON) {
                operations.push_back({a / b, "(" + exprA + "/" + exprB + ")"});
            }
            if (fabs(a) > EPSILON) {
                operations.push_back({b / a, "(" + exprB + "/" + exprA + ")"});
            }
            
            // Recursively try each operation
            for (const auto& op : operations) {
                newNumbers.push_back(op.value);
                newExpressions.push_back(op.expression);
                
                if (solveRecursive(newNumbers, newExpressions, result)) {
                    return true;
                }
                
                newNumbers.pop_back();
                newExpressions.pop_back();
            }
        }
    }
    
    return false;
}

/**
 * Public interface to check if a card set has a solution
 * 
 * @param cards The 4 cards to check
 * @return Solution structure containing result and expression
 */
Solution findSolution(const CardSet& cards) noexcept {
    std::vector<double> numbers(cards.begin(), cards.end());
    std::vector<std::string> expressions;
    expressions.reserve(CARDS_PER_GAME);
    
    for (int card : cards) {
        expressions.push_back(std::to_string(card));
    }
    
    std::string resultExpression;
    bool found = solveRecursive(numbers, expressions, resultExpression);
    
    if (found) {
        // Remove unnecessary outer parentheses
        if (resultExpression.front() == '(' && resultExpression.back() == ')') {
            resultExpression = resultExpression.substr(1, resultExpression.length() - 2);
        }
        return Solution(true, resultExpression);
    }
    
    return Solution(false, "");
}

/**
 * Checks if the current card set has at least one valid solution
 * 
 * @param cards The 4 cards to check
 * @return true if a solution exists
 */
bool hasSolution(const CardSet& cards) noexcept {
    return findSolution(cards).hasSolution;
}

/**
 * Command handler for user input
 */
enum class Command {
    NONE,
    HINT,
    NEW_GAME,
    QUIT,
    INVALID
};

/**
 * Parses user input into a command
 * 
 * @param input User input string
 * @return Parsed command type
 */
Command parseCommand(const std::string& input) noexcept {
    std::string cmd = input;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    if (cmd == "hint" || cmd == "h") return Command::HINT;
    if (cmd == "new" || cmd == "n") return Command::NEW_GAME;
    if (cmd == "quit" || cmd == "q") return Command::QUIT;
    
    return Command::NONE;
}

/**
 * Main game logic
 * Handles game flow, user interaction, and win conditions
 */
class Game {
private:
    CardSet currentCards;
    bool isRunning;
    
    void startNewGame() {
        // Generate a solvable set of cards
        do {
            currentCards = generateCards();
        } while (!hasSolution(currentCards));
        
        displayCards(currentCards);
    }
    
    void showHint() const {
        Solution solution = findSolution(currentCards);
        if (solution.hasSolution) {
            std::cout << "Hint: " << solution.expression << " = " << TARGET_VALUE << std::endl;
        } else {
            std::cout << "Hint: No solution exists (this shouldn't happen!)" << std::endl;
        }
    }
    
    bool processExpression(const std::string& input) {
        std::string cleanInput = input;
        cleanInput.erase(remove(cleanInput.begin(), cleanInput.end(), ' '), cleanInput.end());

        // Validate Parentheses
        if (!validateParentheses(cleanInput)) {
            return false;
        }
        
        // Validate card usage
        if (!validateCardUsage(cleanInput, currentCards)) {
            std::cout << "Error: You must use each card exactly once." << std::endl;
            std::cout << "Available cards: ";
            for (int card : currentCards) std::cout << card << " ";
            std::cout << std::endl;
            return false;
        }
        
        // Evaluate expression
        double result = evaluateExpression(cleanInput);
        
        if (std::isfinite(result) && fabs(result - TARGET_VALUE) < EPSILON) {
            std::cout << "✓ Correct! " << cleanInput << " = " << TARGET_VALUE << std::endl;
            return true;
        } else {
            std::cout << "✗ Result = " << result << ", not " << TARGET_VALUE << ". Try again!" << std::endl;
            return false;
        }
    }
    
public:
    Game() : isRunning(true) {
        std::cout << "======== 24 Point Game ========" << std::endl;
        std::cout << "Rules: Use each of the 4 numbers exactly once." << std::endl;
        std::cout << "Combine them with +, -, *, / and parentheses to make " << TARGET_VALUE << "." << std::endl;
        std::cout << "Numbers: A=1, J=11, Q=12, K=13" << std::endl;
        std::cout << "Commands: hint, new, quit" << std::endl;
        std::cout << "================================" << std::endl << std::endl;
    }
    
    void run() {
        startNewGame();
        
        while (isRunning) {
            std::cout << "> ";
            std::string input;
            getline(std::cin, input);
            
            if (input.empty()) continue;
            
            Command cmd = parseCommand(input);
            
            switch (cmd) {
                case Command::QUIT:
                    isRunning = false;
                    std::cout << "Thanks for playing!" << std::endl;
                    break;
                    
                case Command::NEW_GAME:
                    std::cout << "\n=== New Game ===" << std::endl;
                    startNewGame();
                    break;
                    
                case Command::HINT:
                    showHint();
                    break;
                    
                case Command::NONE:
                    if (processExpression(input)) {
                        std::cout << "\n=== Congratulations! Starting new game... ===\n" << std::endl;
                        startNewGame();
                    }
                    break;
                    
                default:
                    std::cout << "Unknown command. Available: hint, new, quit" << std::endl;
                    break;
            }
        }
    }
};

/**
 * Program entry point
 */
int main() {
    try {
        Game game;
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}