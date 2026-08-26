#include "../../include/AppConfig.h"
#include <Preferences.h>
class ConfigManager
{
public:
    bool begin();
    void load(AppConfig& config);
    void save(const AppConfig& config);

private:
    Preferences prefs;
};