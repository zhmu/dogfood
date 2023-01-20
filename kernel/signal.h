#pragma once

#include <dogfood/signal.h>
#include <array>
#include <bitset>

namespace amd64 { struct TrapFrame; }

namespace signal {
    struct Action
    {
        Action() = default;
        Action(const struct sigaction&);

        struct sigaction ToSigAction() const;

        using Handler = void(*)(int);
        Handler handler = SIG_DFL;
        using Restorer = void(*)();
        Restorer restorer{};
        sigset_t mask{};
        int flags{};
    };

    struct State
    {
        std::bitset<NSIG - 1> pending;
        sigset_t mask{};
        std::array<Action, NSIG - 1> action;
    };

    bool Send(process::Process&, int signo);
    int kill(amd64::TrapFrame& tf);
    int sigaction(amd64::TrapFrame& tf);
    int sigreturn(amd64::TrapFrame& tf);
    int sigprocmask(amd64::TrapFrame& tf);
}
