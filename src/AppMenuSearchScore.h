/*
 * Copyright (C) 2026 Guido Iodice <guido[dot]iodice[at]gmail[dot]com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 or any later version.
 */

#pragma once

#include <QString>

namespace Material::AppMenuSearchPrivate
{

int calculateFuzzyScore(const QString &pattern, const QString &text, const QString &patternLower = QString());

} // namespace Material::AppMenuSearchPrivate
