#pragma once

#include "../Match_Engine/Order.h"

struct PriceLevel {

    Order* head = nullptr;

    Order* tail = nullptr;
};