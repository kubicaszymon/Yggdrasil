//
// Created by Szymon on 21/10/2026.
//

#include "libperun32.h"

struct Perun32::pImplPerun32 {
};

Perun32::Perun32()
    : pImpl(std::make_unique<Perun32::pImplPerun32>()){}

void Perun32::Enumerate() {
    int a = 3;
}
