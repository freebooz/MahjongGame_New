#include "GuiyangMahjong.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMahjongServer);
DEFINE_LOG_CATEGORY(LogMahjongNet);
DEFINE_LOG_CATEGORY(LogMahjongRule);
DEFINE_LOG_CATEGORY(LogMahjongScore);
DEFINE_LOG_CATEGORY(LogMahjongUI);
DEFINE_LOG_CATEGORY(LogMahjongAndroid);
DEFINE_LOG_CATEGORY(LogMahjongReconnect);
DEFINE_LOG_CATEGORY(LogMahjongMCP);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, GuiyangMahjong, "GuiyangMahjong");
