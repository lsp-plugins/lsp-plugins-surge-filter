/*
 * Copyright (C) 2021 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2021 Vladimir Sadovnikov <sadko4u@gmail.com>
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
#define LSP_PLUGINS_SURGE_FILTER_VERSION_MICRO       3

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
        static const int surge_filter_classes[] = { C_DYNAMICS, -1 };

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
            COMBO("modein", "Fade in mode", 3, surge_modes),      \
            COMBO("modeout", "Fade out mode", 3, surge_modes),      \
            AMP_GAIN("input", "Input gain", 1.0f, GAIN_AMP_P_24_DB), \
            EXT_LOG_CONTROL("thr_on", "Threshold for switching on", U_GAIN_AMP, surge_filter_metadata::THRESH), \
            EXT_LOG_CONTROL("thr_off", "Threshold for switching off", U_GAIN_AMP, surge_filter_metadata::THRESH), \
            LOG_CONTROL("rms", "RMS estimation time", U_MSEC, surge_filter_metadata::RMS), \
            CONTROL("fadein", "Fade in time", U_MSEC, surge_filter_metadata::FADEIN), \
            CONTROL("fadeout", "Fade out time", U_MSEC, surge_filter_metadata::FADEOUT), \
            CONTROL("fidelay", "Fade in cancel delay time", U_MSEC, surge_filter_metadata::PAUSE), \
            CONTROL("fodelay", "Fade out cancel delay time", U_MSEC, surge_filter_metadata::PAUSE), \
            BLINK("active", "Activity indicator"), \
            AMP_GAIN("output", "Output gain", 1.0f, GAIN_AMP_P_24_DB), \
            MESH("ig", "Input signal graph", channels+1, surge_filter_metadata::MESH_POINTS), \
            MESH("og", "Output signal graph", channels+1, surge_filter_metadata::MESH_POINTS), \
            MESH("grg", "Gain reduction graph", 2, surge_filter_metadata::MESH_POINTS), \
            MESH("eg", "Envelope graph", 2, surge_filter_metadata::MESH_POINTS), \
            SWITCH("grv", "Gain reduction visibility", 1.0f), \
            SWITCH("ev", "Envelope visibility", 1.0f), \
            METER_GAIN("grm", "Gain reduction meter", GAIN_AMP_P_24_DB), \
            METER_GAIN("em", "Envelope meter", GAIN_AMP_P_24_DB)

        static const port_t surge_filter_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,
            BYPASS,
            SURGE_FILTER_COMMON(1),
            SWITCH("igv", "Input graph visibility", 1.0f),
            SWITCH("ogv", "Output graph visibility", 1.0f),
            METER_GAIN("ilm", "Input level meter", GAIN_AMP_P_24_DB),
            METER_GAIN("olm", "Output level meter", GAIN_AMP_P_24_DB),

            PORTS_END
        };

        static const port_t surge_filter_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,
            BYPASS,
            SURGE_FILTER_COMMON(2),
            SWITCH("igv_l", "Input graph visibility left", 1.0f),
            SWITCH("ogv_l", "Output graph visibility left", 1.0f),
            METER_GAIN("ilm_l", "Input level meter left", GAIN_AMP_P_24_DB),
            METER_GAIN("olm_l", "Output level meter left", GAIN_AMP_P_24_DB),
            SWITCH("igv_r", "Input graph visibility right", 1.0f),
            SWITCH("ogv_r", "Output graph visibility right", 1.0f),
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
            "SF1M",
            &developers::v_sadovnikov,
            "surge_filter_mono",
            LSP_LV2_URI("surge_filter_mono"),
            LSP_LV2UI_URI("surge_filter_mono"),
            "feli",
            LSP_LADSPA_SURGE_FILTER_BASE + 0,
            LSP_LADSPA_URI("surge_filter_mono"),
            LSP_PLUGINS_SURGE_FILTER_VERSION,
            surge_filter_classes,
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
            "SF1S",
            &developers::v_sadovnikov,
            "surge_filter_stereo",
            LSP_LV2_URI("surge_filter_stereo"),
            LSP_LV2UI_URI("surge_filter_stereo"),
            "crjf",
            LSP_LADSPA_SURGE_FILTER_BASE + 1,
            LSP_LADSPA_URI("surge_filter_stereo"),
            LSP_PLUGINS_SURGE_FILTER_VERSION,
            surge_filter_classes,
            E_INLINE_DISPLAY | E_DUMP_STATE,
            surge_filter_stereo_ports,
            "util/surge_filter.xml",
            NULL,
            stereo_plugin_port_groups,
            &surge_filter_bundle
        };
    } // namespace meta
} // namespace lsp
