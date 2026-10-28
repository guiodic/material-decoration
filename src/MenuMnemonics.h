// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

class QMenu;

namespace Material
{
// Attach once to an imported popup; the menu owns the observer.
void manageMenuMnemonics(QMenu *menu);

void synchronizeMenuMnemonicVisibility(bool altPressed);
}
