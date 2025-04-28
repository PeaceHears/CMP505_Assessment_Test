#include "pch.h"
#include "Utils.h"
#include <random>

int Utils::GetRandomInt(int min, int max)
{
    // Use random device as a seed
    std::random_device rd;

    // Mersenne Twister random number generator
    std::mt19937 gen(rd());

    // Uniform distribution in the specified range
    std::uniform_int_distribution<> dis(min, max);

    // Generate and return random number
    return dis(gen);
}