/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "keepaboveoutline_config.h"
#include "ui_keepaboveoutline_config.h"

#include <KColorScheme>
#include <KConfigSkeleton>
#include <KPluginFactory>

K_PLUGIN_CLASS(KeepAboveOutlineConfig)

KeepAboveOutlineConfig::KeepAboveOutlineConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
    , m_ui(new Ui::KeepAboveOutlineConfigForm)
{
    m_ui->setupUi(widget());

    QPalette infoPalette = m_ui->label_info->palette();
    const QColor neutralText = KColorScheme(QPalette::Active, KColorScheme::Window)
                                   .foreground(KColorScheme::NeutralText)
                                   .color();
    infoPalette.setColor(QPalette::WindowText, neutralText);
    m_ui->label_info->setPalette(infoPalette);

    auto *config = new KConfigSkeleton(QStringLiteral("kwinrc"), this);
    config->setCurrentGroup(QStringLiteral("Effect-keep-above-outline"));

    config->addItemBool(QStringLiteral("UseAccentColor"), *new bool, true);
    config->addItemColor(QStringLiteral("CustomColor"), *new QColor, QColor(61, 174, 233));
    config->addItemInt(QStringLiteral("BorderWidth"), *new int, 3);
    config->addItemInt(QStringLiteral("BorderRadius"), *new int, 0);

    addConfig(config, widget());

    // Disable custom color picker when accent color is checked
    auto updateColorEnabled = [this] {
        m_ui->kcfg_CustomColor->setEnabled(!m_ui->kcfg_UseAccentColor->isChecked());
        m_ui->label_customColor->setEnabled(!m_ui->kcfg_UseAccentColor->isChecked());
    };
    connect(m_ui->kcfg_UseAccentColor, &QCheckBox::toggled, this, updateColorEnabled);
    updateColorEnabled();
}

#include "keepaboveoutline_config.moc"
