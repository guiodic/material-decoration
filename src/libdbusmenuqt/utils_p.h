/* This file is part of the dbusmenu-qt library
    SPDX-FileCopyrightText: 2010 Canonical
    SPDX-FileContributor: Aurelien Gateau <aurelien.gateau@canonical.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

class QString;

/**
 * @brief Swaps accelerator mnemonic characters between Qt ('&') and DBusMenu ('_') formats.
 * @param in Input label string.
 * @param src Source mnemonic character to replace.
 * @param dst Replacement mnemonic character.
 * @return String with swapped mnemonic characters.
 */
QString swapMnemonicChar(const QString &in, char src, char dst);
