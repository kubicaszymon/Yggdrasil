//
// Created by Szymon on 21/10/2026.
//

#ifndef libperun32_API
#define libperun32_API
#include <memory>
#include <optional>

class Perun32 {
    public:
    Perun32();
    virtual ~Perun32();

    private:
    struct pImplPerun32;
    std::unique_ptr<pImplPerun32> pImpl;

};

#endif // libperun32_API
