#ifndef PVZ_LAWN_WIDGET_AI_SETTINGS_WIDGET_H
#define PVZ_LAWN_WIDGET_AI_SETTINGS_WIDGET_H

#include "PvZ/SexyAppFramework/Widget/CheckboxListener.h"
#include "PvZ/SexyAppFramework/Widget/Widget.h"

class GameButton;
class VSSetupAddonWidget;

namespace Sexy {
class Checkbox;
class WidgetManager;
} // namespace Sexy

class AISettingsWidget final : public Sexy::Widget, public Sexy::CheckboxListener {
public:
    explicit AISettingsWidget(VSSetupAddonWidget *owner);
    ~AISettingsWidget();

    void AddedToManager(Sexy::WidgetManager *manager);
    void RemovedFromManager(Sexy::WidgetManager *manager);
    void Draw(Sexy::Graphics *graphics);
    void CheckboxChecked(int theId, bool checked) override;
    void SyncState();
    void SetDisabled(bool disabled);

private:
    VSSetupAddonWidget *mOwner = nullptr;
    Sexy::Checkbox *mPlantAICheckbox = nullptr;
    Sexy::Checkbox *mZombieAICheckbox = nullptr;
    Sexy::Checkbox *mEnhancementCheckbox = nullptr;
    Sexy::Checkbox *mManualDraftCheckbox = nullptr;
    Sexy::Checkbox *mDisableTemplatesCheckbox = nullptr;
    GameButton *mCloseButton = nullptr;
    bool mDisabled = false;

    void _destructor();
    void _destructor2();
};

#endif // PVZ_LAWN_WIDGET_AI_SETTINGS_WIDGET_H
