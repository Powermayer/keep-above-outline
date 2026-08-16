/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <KCModule>

namespace Ui
{
class KeepAboveOutlineConfigForm;
}

class KeepAboveOutlineConfig : public KCModule
{
    Q_OBJECT

public:
    explicit KeepAboveOutlineConfig(QObject *parent, const KPluginMetaData &data);

    void save() override;

private:
    Ui::KeepAboveOutlineConfigForm *m_ui;
};
