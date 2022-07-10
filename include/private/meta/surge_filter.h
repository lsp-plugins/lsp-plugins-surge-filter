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

#ifndef PRIVATE_META_SURGE_FILTER_H_
#define PRIVATE_META_SURGE_FILTER_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>


namespace lsp
{
    namespace meta
    {
        struct surge_filter_metadata
        {
            static constexpr float THRESH_MIN       = GAIN_AMP_M_120_DB;
            static constexpr float THRESH_MAX       = GAIN_AMP_M_24_DB;
            static constexpr float THRESH_DFL       = GAIN_AMP_M_72_DB;
            static constexpr float THRESH_STEP      = 0.1f;

            static constexpr float RMS_MIN          = 4.0f;
            static constexpr float RMS_MAX          = 100.0f;
            static constexpr float RMS_DFL          = 10.0f;
            static constexpr float RMS_STEP         = 0.01f;

            static constexpr float FADEIN_MIN       = 0.0f;
            static constexpr float FADEIN_MAX       = 1000.0f;
            static constexpr float FADEIN_DFL       = 100.0f;
            static constexpr float FADEIN_STEP      = 0.5f;

            static constexpr float FADEOUT_MIN      = 0.0f;
            static constexpr float FADEOUT_MAX      = 500.0f;
            static constexpr float FADEOUT_DFL      = 0.0f;
            static constexpr float FADEOUT_STEP     = 0.5f;

            static constexpr float PAUSE_MIN        = 0.0f;
            static constexpr float PAUSE_MAX        = 100.0f;
            static constexpr float PAUSE_DFL        = 10.0f;
            static constexpr float PAUSE_STEP       = 0.5f;

            static constexpr size_t MESH_POINTS     = 640;
            static constexpr float MESH_TIME        = 5.0f;
        };

        extern const meta::plugin_t surge_filter_mono;
        extern const meta::plugin_t surge_filter_stereo;
    } // namespace meta
} // namespace lsp


#endif /* PRIVATE_META_SURGE_FILTER_H_ */
