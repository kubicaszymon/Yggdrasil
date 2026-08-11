#include <iostream>

int main() {

    const auto language = "C++";
    std::cout << "Hello and welcome to " << language << "!\n";

    for (int i = 1; i <= 5; i++)
    {
        std::cout << "i = " << i << std::endl;
    }

    return 0;
}