/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-surge-filter
 * Created on: 3 авг. 2021 г.
 *
 * lsp-plugins-surge-filter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-surge-filter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-surge-filter. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/surge_filter.h>

#define LSP_PLUGINS_SURGE_FILTER_VERSION_MAJOR       1
#define LSP_PLUGINS_SURGE_FILTER_VERSION_MINOR       0
#define LSP_PLUGINS_SURGE_FILTER_VERSION_MICRO       29

#define LSP_PLUGINS_SURGE_FILTER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_SURGE_FILTER_VERSION_MAJOR, \
        LSP_PLUGINS_SURGE_FILTER_VERSION_MINOR, \
        LSP_PLUGINS_SURGE_FILTER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        static const int plugin_classes[]           = { C_DYNAMICS, -1 };
        static const int clap_features_mono[]       = { CF_AUDIO_EFFECT, CF_UTILITY, CF_MONO, -1 };
        static const int clap_features_stereo[]     = { CF_AUDIO_EFFECT, CF_UTILITY, CF_STEREO, -1 };

        static const port_item_t surge_modes[] =
        {
            { "Linear",         "surge.linear"          },
            { "Cubic",          "surge.cubic"           },
            { "Sine",           "surge.sine"            },
            { "Gaussian",       "surge.gaussian"        },
            { "Parabolic",      "surge.parabolic"       },
            { NULL, NULL }
        };

        #define SURGE_FILTER_COMMON(channels)    \
            COMBO("modein", "Fade in mode", "Fadein mode", 3, surge_modes),      \
            COMBO("modeout", "Fade out mode", "Fadeout mode", 3, surge_modes),      \
            AMP_GAIN("input", "Input gain", "Input gain", 1.0f, GAIN_AMP_P_24_DB), \
            EXT_LOG_CONTROL("thr_on", "Threshold for switching on", "On threshold", U_GAIN_AMP, surge_filter_metadata::THRESH), \
            EXT_LOG_CONTROL("thr_off", "Threshold for switching off", "Off threshold", U_GAIN_AMP, surge_filter_metadata::THRESH), \
            LOG_CONTROL("rms", "RMS estimation time", "RMS time", U_MSEC, surge_filter_metadata::RMS), \
            CONTROL("fadein", "Fade in time", "Fade in", U_MSEC, surge_filter_metadata::FADEIN), \
            CONTROL("fadeout", "Fade out time", "Fade out", U_MSEC, surge_filter_metadata::FADEOUT), \
            CONTROL("fidelay", "Fade in cancel delay time", "Fade in cancel", U_MSEC, surge_filter_metadata::PAUSE), \
            CONTROL("fodelay", "Fade out cancel delay time", "Fade out cancel", U_MSEC, surge_filter_metadata::PAUSE), \
            BLINK("active", "Activity indicator"), \
            AMP_GAIN("output", "Output gain", "Output gain", 1.0f, GAIN_AMP_P_24_DB), \
            MESH("ig", "Input signal graph", channels+1, surge_filter_metadata::MESH_POINTS + 2), \
            MESH("og", "Output signal graph", channels+1, surge_filter_metadata::MESH_POINTS), \
            MESH("grg", "Gain reduction graph", 2, surge_filter_metadata::MESH_POINTS + 4), \
            MESH("eg", "Envelope graph", 2, surge_filter_metadata::MESH_POINTS), \
            SWITCH("grv", "Gain reduction visibility", "Show reduct", 1.0f), \
            SWITCH("ev", "Envelope visibility", "Show env", 1.0f), \
            METER_GAIN("grm", "Gain reduction meter", GAIN_AMP_P_24_DB), \
            METER_GAIN("em", "Envelope meter", GAIN_AMP_P_24_DB)

        static const port_t surge_filter_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            BYPASS,
            SURGE_FILTER_COMMON(1),
            SWITCH("igv", "Input graph visibility", "Show in", 1.0f),
            SWITCH("ogv", "Output graph visibility", "Show out", 1.0f),
            METER_GAIN("ilm", "Input level meter", GAIN_AMP_P_24_DB),
            METER_GAIN("olm", "Output level meter", GAIN_AMP_P_24_DB),

            PORTS_END
        };

        static const port_t surge_filter_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            BYPASS,
            SURGE_FILTER_COMMON(2),
            SWITCH("igv_l", "Input graph visibility left", "Show in L", 1.0f),
            SWITCH("ogv_l", "Output graph visibility left", "Show out L", 1.0f),
            METER_GAIN("ilm_l", "Input level meter left", GAIN_AMP_P_24_DB),
            METER_GAIN("olm_l", "Output level meter left", GAIN_AMP_P_24_DB),
            SWITCH("igv_r", "Input graph visibility right", "Show in R", 1.0f),
            SWITCH("ogv_r", "Output graph visibility right", "Show out R", 1.0f),
            METER_GAIN("ilm_r", "Input level meter right", GAIN_AMP_P_24_DB),
            METER_GAIN("olm_r", "Output level meter right", GAIN_AMP_P_24_DB),

            PORTS_END
        };

        const meta::bundle_t surge_filter_bundle =
        {
            "surge_filter",
            "Surge Filter",
            B_UTILITIES,
            "CuySiF1VSj8",
            "This plugin is designed mostly as a workaround for systems which don't support\nsmooth fade-ins and fade-outs of audio stream on playback start and stop events.\nSuch events may produce noticeable pops, especially when the audio stream is\nadditionally amplified."
        };

        const meta::plugin_t surge_filter_mono =
        {
            "Sprungfilter Mono",
            "Surge Filter Mono",
            "Surge Filter Mono",
            "SF1M",
            &developers::v_sadovnikov,
            "surge_filter_mono",
            {
                LSP_LV2_URI("surge_filter_mono"),
                LSP_LV2UI_URI("surge_filter_mono"),
                "feli",
                LSP_VST3_UID("sf1m    feli"),
                LSP_VST3UI_UID("sf1m    feli"),
                LSP_LADSPA_SURGE_FILTER_BASE + 0,
                LSP_LADSPA_URI("surge_filter_mono"),
                LSP_CLAP_URI("surge_filter_mono"),
                LSP_GST_UID("surge_filter_mono"),
            },
            LSP_PLUGINS_SURGE_FILTER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            surge_filter_mono_ports,
            "util/surge_filter.xml",
            NULL,
            mono_plugin_port_groups,
            &surge_filter_bundle
        };

        const meta::plugin_t surge_filter_stereo =
        {
            "Sprungfilter Stereo",
            "Surge Filter Stereo",
            "Surge Filter Stereo",
            "SF1S",
            &developers::v_sadovnikov,
            "surge_filter_stereo",
            {
                LSP_LV2_URI("surge_filter_stereo"),
                LSP_LV2UI_URI("surge_filter_stereo"),
                "crjf",
                LSP_VST3_UID("sf1s    crjf"),
                LSP_VST3UI_UID("sf1s    crjf"),
                LSP_LADSPA_SURGE_FILTER_BASE + 1,
                LSP_LADSPA_URI("surge_filter_stereo"),
                LSP_CLAP_URI("surge_filter_stereo"),
                LSP_GST_UID("surge_filter_stereo"),
            },
            LSP_PLUGINS_SURGE_FILTER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            surge_filter_stereo_ports,
            "util/surge_filter.xml",
            NULL,
            stereo_plugin_port_groups,
            &surge_filter_bundle
        };
    } /* namespace meta */
} /* namespace lsp */
