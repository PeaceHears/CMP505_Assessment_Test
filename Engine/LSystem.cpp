#include "pch.h"
#include "LSystem.h"

LSystem::LSystem(const std::string& axiom, const std::vector<std::pair<char, std::string>>& rules, int iterations)
    : currentString(axiom), productionRules(rules), iterations(iterations) {}

void LSystem::Generate()
{
    for (int i = 0; i < iterations; ++i)
    {
        std::string nextString;

        for (char c : currentString)
        {
            bool ruleFound = false;

            for (const auto& rule : productionRules)
            {
                if (c == rule.first)
                {
                    nextString += rule.second;
                    ruleFound = true;
                    break;
                }
            }

            if (!ruleFound)
            {
                nextString += c;
            }
        }
        currentString = nextString;
    }
}

const std::string& LSystem::GetCurrentString() const
{
    return currentString;
}
