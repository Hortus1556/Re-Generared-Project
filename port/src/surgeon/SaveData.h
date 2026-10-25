// The player's progress: per level its grade (1-6, 0 not done), best score
// and time. The game's scripts own it (_root.level<n>, score<n>, timer<n>
// and their SharedObject, kept as the game's save file; FlashClasses.h):
// load() reads it from them, save() writes it back and flushes, as the Flash
// operation's won and lost frames did (FlashSurgeonGame).
#pragma once

#include <array>
#include <functional>

struct SaveData {
    static constexpr int kLevels = 31;  // the arrays the Flash games kept (AS1 uses 31, Christmas 5)

    std::array<int, kLevels + 1> grade{};     // 1-based
    std::array<double, kLevels + 1> score{};
    std::array<double, kLevels + 1> timer{};  // movie frames

    // Totals over the levels (the Flash _root.score and _root.timer).
    double totalScore() const;
    double totalTimer() const;

    void load() {
        grade.fill(0);
        score.fill(0);
        timer.fill(0);
        if (loader) loader(*this);
    }
    void save() {
        if (saver) saver(*this);
    }

    // Where it comes from and goes (the game's scripts).
    std::function<void(SaveData&)> loader;
    std::function<void(const SaveData&)> saver;
};
