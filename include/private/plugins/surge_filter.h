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

#ifndef PRIVATE_PLUGINS_SURGE_FILTER_H_
#define PRIVATE_PLUGINS_SURGE_FILTER_H_

#include <lsp-plug.in/plug-fw/plug.h>
#include <lsp-plug.in/plug-fw/core/IDBuffer.h>
#include <lsp-plug.in/dsp-units/ctl/Blink.h>
#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/util/Depopper.h>
#include <lsp-plug.in/dsp-units/util/MeterGraph.h>

#include <private/meta/surge_filter.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Surge Filter Plugin Series
         */
        class surge_filter: public plug::Module
        {
            protected:
                typedef struct channel_t
                {
                    float              *vIn;            // Input buffer
                    float              *vOut;           // Output buffer
                    float              *vBuffer;        // Buffer for processing
                    dspu::Bypass        sBypass;        // Bypass
                    dspu::Delay         sDelay;         // Delay for latency compensation
                    dspu::Delay         sDryDelay;      // Dry delay
                    dspu::MeterGraph    sIn;            // Input metering graph
                    dspu::MeterGraph    sOut;           // Output metering graph
                    bool                bInVisible;     // Input signal visibility flag
                    bool                bOutVisible;    // Output signal visibility flag

                    plug::IPort        *pIn;            // Input port
                    plug::IPort        *pOut;           // Output port
                    plug::IPort        *pInVisible;     // Input visibility
                    plug::IPort        *pOutVisible;    // Output visibility
                    plug::IPort        *pMeterIn;       // Input Meter
                    plug::IPort        *pMeterOut;      // Output Meter
                } channel_t;

            protected:
                size_t              nChannels;          // Number of channels
                channel_t          *vChannels;          // Array of channels
                float              *vBuffer;            // Buffer for processing
                float              *vEnv;               // Envelope
                float              *vTimePoints;        // Time points
                float               fGainIn;            // Input gain
                float               fGainOut;           // Output gain
                bool                bGainVisible;       // Gain visible
                bool                bEnvVisible;        // Envelope visible
                uint8_t            *pData;              // Allocated data
                core::IDBuffer     *pIDisplay;          // Inline display buffer

                dspu::MeterGraph    sGain;              // Gain metering graph
                dspu::MeterGraph    sEnv;               // Envelop metering graph
                dspu::Blink         sActive;            // Activity indicator
                dspu::Depopper      sDepopper;          // Depopper module

                plug::IPort        *pModeIn;            // Mode for fade in
                plug::IPort        *pModeOut;           // Mode for fade out
                plug::IPort        *pGainIn;            // Input gain
                plug::IPort        *pGainOut;           // Output gain
                plug::IPort        *pThreshOn;          // Threshold
                plug::IPort        *pThreshOff;         // Threshold
                plug::IPort        *pRmsLen;            // RMS estimation length
                plug::IPort        *pFadeIn;            // Fade in time
                plug::IPort        *pFadeOut;           // Fade out time
                plug::IPort        *pFadeInDelay;       // Fade in time
                plug::IPort        *pFadeOutDelay;      // Fade out time
                plug::IPort        *pActive;            // Active flag
                plug::IPort        *pBypass;            // Bypass port
                plug::IPort        *pMeshIn;            // Input mesh
                plug::IPort        *pMeshOut;           // Output mesh
                plug::IPort        *pMeshGain;          // Gain mesh
                plug::IPort        *pMeshEnv;           // Envelope mesh
                plug::IPort        *pGainVisible;       // Gain mesh visibility
                plug::IPort        *pEnvVisible;        // Envelope mesh visibility
                plug::IPort        *pGainMeter;         // Gain reduction meter
                plug::IPort        *pEnvMeter;          // Envelope meter

            public:
                explicit            surge_filter(const meta::plugin_t *metadata, size_t channels);
                virtual            ~surge_filter();

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports);
                virtual void        destroy();

            public:
                virtual void        update_sample_rate(long sr);
                virtual void        update_settings();
                virtual void        process(size_t samples);
                virtual bool        inline_display(plug::ICanvas *cv, size_t width, size_t height);
                virtual void        dump(dspu::IStateDumper *v) const;
        };
    } // namespace plugins
} // namespace lsp

#endif /* PRIVATE_PLUGINS_SURGE_FILTER_H_ */
