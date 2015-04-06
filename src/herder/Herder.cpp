#pragma once

#include "herder/Herder.h"

namespace stellar
{

VirtualClock::duration const Herder::EXP_LEDGER_TIMESPAN_SECONDS = std::chrono::seconds(5);
VirtualClock::duration const Herder::MAX_SCP_TIMEOUT_SECONDS = std::chrono::seconds(240);
VirtualClock::duration const Herder::MAX_TIME_SLIP_SECONDS = std::chrono::seconds(60);
VirtualClock::duration const Herder::NODE_EXPIRATION_SECONDS = std::chrono::seconds(240);
uint32 const Herder::LEDGER_VALIDITY_BRACKET = 1000;


}
