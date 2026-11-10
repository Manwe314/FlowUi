#pragma once

/** @namespace FlowUi::FSEL
 * @brief Standard FlowUi elements, parameter models and theme customizations.
 */

/**
 * @file FSEL.hpp
 * Public umbrella header for the FlowUi standard element library.
 *
 * Register FSELTheme with the app's ThemeManager before using themed controls.
 * Definition tags such as kButton create typed ElementBuilder instances through
 * UiManager::createElement(); parameter models remain application-owned.
 */

#include "FSEL/Theme.hpp"
#include "FSEL/Box.hpp"
#include "FSEL/Button.hpp"
#include "FSEL/Checkbox.hpp"
#include "FSEL/ComboBox.hpp"
#include "FSEL/DragValue.hpp"
#include "FSEL/Label.hpp"
#include "FSEL/NumberInput.hpp"
#include "FSEL/Numeric.hpp"
#include "FSEL/PopupSurface.hpp"
#include "FSEL/ProgressBar.hpp"
#include "FSEL/RadioChoice.hpp"
#include "FSEL/SelectableSurface.hpp"
#include "FSEL/Slider.hpp"
#include "FSEL/Spacer.hpp"
#include "FSEL/SplitterHandle.hpp"
#include "FSEL/Switch.hpp"
#include "FSEL/StandardIcons.hpp"
#include "FSEL/TextArea.hpp"
#include "FSEL/TextInput.hpp"
