#pragma once
#include <string>
#include <vector>
#include <stack>

class LSystem
{
public:
    LSystem(const std::string& axiom, const std::vector<std::pair<char, std::string>>& rules, int iterations);
    void Generate();
    const std::string& GetCurrentString() const;

private:
    std::string currentString;
    std::vector<std::pair<char, std::string>> productionRules;
    int iterations;
};