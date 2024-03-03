/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
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

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/shared/debug.h>
#include <lsp-plug.in/shared/id_colors.h>
#include <lsp-plug.in/stdlib/math.h>

#include <private/plugins/surge_filter.h>

#define BUFFER_SIZE     0x1000

namespace lsp
{
    namespace plugins
    {
        //-------------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::surge_filter_mono,
            &meta::surge_filter_stereo
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new surge_filter(meta, (meta == &meta::surge_filter_stereo) ? 2 : 1);
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //-------------------------------------------------------------------------
        surge_filter::surge_filter(const meta::plugin_t *metadata, size_t channels): plug::Module(metadata)
        {
            nChannels       = channels;
            vChannels       = NULL;
            vBuffer         = NULL;
            vEnv            = NULL;
            vTimePoints     = 0;
            fGainIn         = 1.0f;
            fGainOut        = 1.0f;
            bGainVisible    = false;
            bEnvVisible     = false;
            pData           = NULL;
            pIDisplay       = NULL;

            pModeIn         = NULL;
            pModeOut        = NULL;
            pGainIn         = NULL;
            pGainOut        = NULL;
            pThreshOn       = NULL;
            pThreshOff      = NULL;
            pRmsLen         = NULL;
            pFadeIn         = NULL;
            pFadeOut        = NULL;
            pFadeInDelay    = NULL;
            pFadeOutDelay   = NULL;
            pActive         = NULL;
            pBypass         = NULL;
            pMeshIn         = NULL;
            pMeshOut        = NULL;
            pMeshGain       = NULL;
            pMeshEnv        = NULL;
            pGainVisible    = NULL;
            pEnvVisible     = NULL;
            pGainMeter      = NULL;
            pEnvMeter       = NULL;
        }

        surge_filter::~surge_filter()
        {
            do_destroy();
        }

        void surge_filter::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            plug::Module::init(wrapper, ports);

            // Allocate buffers
            size_t meshbuf      = align_size(meta::surge_filter_metadata::MESH_POINTS, DEFAULT_ALIGN);
            size_t to_alloc     = 2*BUFFER_SIZE + nChannels * BUFFER_SIZE + meshbuf;
            float *bufs         = alloc_aligned<float>(pData, to_alloc);
            if (bufs == NULL)
                return;

            // Allocate channels
            vChannels       = new channel_t[nChannels];
            if (vChannels == NULL)
                return;
            vBuffer         = advance_ptr_bytes<float>(bufs, BUFFER_SIZE * sizeof(float));
            vEnv            = advance_ptr_bytes<float>(bufs, BUFFER_SIZE * sizeof(float));
            vTimePoints     = advance_ptr_bytes<float>(bufs, meshbuf * sizeof(float));

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->sBypass.construct();

                c->vIn          = NULL;
                c->vOut         = NULL;
                c->vBuffer      = advance_ptr_bytes<float>(bufs, BUFFER_SIZE * sizeof(float));
                c->bInVisible   = true;
                c->bOutVisible  = true;
            }

            // Initialize de-popper
            sDepopper.construct();
            sGain.set_method(dspu::MM_MINIMUM);

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pIn);

            // Bind output audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pOut);

            // Bind control ports
            BIND_PORT(pBypass);
            BIND_PORT(pModeIn);
            BIND_PORT(pModeOut);
            BIND_PORT(pGainIn);
            BIND_PORT(pThreshOn);
            BIND_PORT(pThreshOff);
            BIND_PORT(pRmsLen);
            BIND_PORT(pFadeIn);
            BIND_PORT(pFadeOut);
            BIND_PORT(pFadeInDelay);
            BIND_PORT(pFadeOutDelay);
            BIND_PORT(pActive);
            BIND_PORT(pGainOut);
            BIND_PORT(pMeshIn);
            BIND_PORT(pMeshOut);
            BIND_PORT(pMeshGain);
            BIND_PORT(pMeshEnv);
            BIND_PORT(pGainVisible);
            BIND_PORT(pEnvVisible);
            BIND_PORT(pGainMeter);
            BIND_PORT(pEnvMeter);

            // Bind custom channel ports
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                BIND_PORT(c->pInVisible);
                BIND_PORT(c->pOutVisible);
                BIND_PORT(c->pMeterIn);
                BIND_PORT(c->pMeterOut);
            }

            // Initialize time points
            float delta     = meta::surge_filter_metadata::MESH_TIME / (meta::surge_filter_metadata::MESH_POINTS - 1);
            for (size_t i=0; i<meta::surge_filter_metadata::MESH_POINTS; ++i)
                vTimePoints[i]  = meta::surge_filter_metadata::MESH_TIME - i*delta;
        }

        void surge_filter::destroy()
        {
            plug::Module::destroy();
            do_destroy();
        }

        void surge_filter::do_destroy()
        {
            // Drop all channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sDelay.destroy();
                    c->sDryDelay.destroy();
                    c->sIn.destroy();
                    c->sOut.destroy();
                }

                delete [] vChannels;
                vChannels = NULL;
            }

            // Drop buffers
            if (pData != NULL)
            {
                free_aligned(pData);
                pData   = NULL;
            }

            // Drop inline display buffer
            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }
        }

        void surge_filter::update_sample_rate(long sr)
        {
            size_t samples_per_dot  = dspu::seconds_to_samples(sr, meta::surge_filter_metadata::MESH_TIME / meta::surge_filter_metadata::MESH_POINTS);
            size_t max_delay        = dspu::millis_to_samples(sr, meta::surge_filter_metadata::FADEOUT_MAX);

            sDepopper.init(sr, meta::surge_filter_metadata::FADEOUT_MAX, meta::surge_filter_metadata::RMS_MAX);
            sGain.init(meta::surge_filter_metadata::MESH_POINTS, samples_per_dot);
            sEnv.init(meta::surge_filter_metadata::MESH_POINTS, samples_per_dot);
            sActive.init(sr);

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->sBypass.init(sr);
                c->sDelay.init(max_delay);
                c->sDryDelay.init(max_delay);
                c->sIn.init(meta::surge_filter_metadata::MESH_POINTS, samples_per_dot);
                c->sOut.init(meta::surge_filter_metadata::MESH_POINTS, samples_per_dot);
            }
        }

        void surge_filter::update_settings()
        {
            bool bypass     = pBypass->value() >= 0.5f;
            fGainIn         = pGainIn->value();
            fGainOut        = pGainOut->value();
            bGainVisible    = pGainVisible->value() >= 0.5f;
            bEnvVisible     = pEnvVisible->value() >= 0.5f;

            // Change depopper state
            sDepopper.set_fade_in_mode(dspu::depopper_mode_t(pModeIn->value()));
            sDepopper.set_fade_in_threshold(pThreshOn->value());
            sDepopper.set_fade_in_time(pFadeIn->value());
            sDepopper.set_fade_in_delay(pFadeInDelay->value());
            sDepopper.set_fade_out_mode(dspu::depopper_mode_t(pModeOut->value()));
            sDepopper.set_fade_out_threshold(pThreshOff->value());
            sDepopper.set_fade_out_time(pFadeOut->value());
            sDepopper.set_fade_out_delay(pFadeOutDelay->value());
            sDepopper.set_rms_length(pRmsLen->value());
            sDepopper.reconfigure();

            size_t latency  = sDepopper.latency();

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];

                c->sBypass.set_bypass(bypass);
                c->sDelay.set_delay(latency);
                c->sDryDelay.set_delay(latency);
                c->bInVisible   = c->pInVisible->value();
                c->bOutVisible  = c->pOutVisible->value();
            }

            // Report actual latency
            set_latency(latency);
        }

        void surge_filter::process(size_t samples)
        {
            // Bind ports
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->vIn          = c->pIn->buffer<float>();
                c->vOut         = c->pOut->buffer<float>();
            }

            for (size_t nleft=samples; nleft > 0; )
            {
                size_t to_process = (nleft > BUFFER_SIZE) ? BUFFER_SIZE : nleft;

                // Perform main processing
                if (nChannels > 1)
                {
                    // Apply input gain
                    dsp::mul_k3(vChannels[0].vBuffer, vChannels[0].vIn, fGainIn, to_process);
                    dsp::mul_k3(vChannels[1].vBuffer, vChannels[1].vIn, fGainIn, to_process);

                    // Process input graph
                    vChannels[0].sIn.process(vChannels[0].vBuffer, to_process);
                    vChannels[1].sIn.process(vChannels[1].vBuffer, to_process);

                    // Apply meter values
                    vChannels[0].pMeterIn->set_value(dsp::abs_max(vChannels[0].vBuffer, to_process));
                    vChannels[1].pMeterIn->set_value(dsp::abs_max(vChannels[1].vBuffer, to_process));

                    // Compute control signal
                    dsp::pamax3(vBuffer, vChannels[0].vBuffer, vChannels[1].vBuffer, to_process);
                }
                else
                {
                    // Apply input gain
                    dsp::mul_k3(vChannels[0].vBuffer, vChannels[0].vIn, fGainIn, to_process);

                    // Process input graph and meter
                    vChannels[0].sIn.process(vChannels[0].vBuffer, to_process);
                    vChannels[0].pMeterIn->set_value(dsp::abs_max(vChannels[0].vBuffer, to_process));

                    // Compute control signal
                    dsp::abs2(vBuffer, vChannels[0].vBuffer, to_process);
                }

                // Process the gain reduction control
                sDepopper.process(vEnv, vBuffer, vBuffer, to_process);
                pGainMeter->set_value(dsp::abs_min(vBuffer, to_process));
                pEnvMeter->set_value(dsp::abs_max(vEnv, to_process));
                sGain.process(vBuffer, to_process);
                sEnv.process(vEnv, to_process);

                // Apply reduction to the signal
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];

                    // Apply delay to compensate latency and output gain
                    c->sDelay.process(c->vBuffer, c->vBuffer, to_process);
                    c->sDryDelay.process(c->vOut, c->vIn, to_process);
                    dsp::fmmul_k3(c->vBuffer, vBuffer, fGainOut, to_process);
                    c->sBypass.process(c->vOut, c->vOut, c->vBuffer, to_process);

                    // Process output graph and meter
                    c->sOut.process(c->vBuffer, to_process);
                    c->pMeterOut->set_value(dsp::abs_max(c->vBuffer, to_process));

                    // Update pointers
                    c->vIn         += to_process;
                    c->vOut        += to_process;
                }

                // Update number of samples left
                nleft      -= to_process;
            }

            // Sync gain mesh
            plug::mesh_t *mesh    = pMeshGain->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()) && (bGainVisible))
            {
                float *x    = mesh->pvData[0];
                float *y    = mesh->pvData[1];

                dsp::copy(&x[2], vTimePoints, meta::surge_filter_metadata::MESH_POINTS);
                dsp::copy(&y[2], sGain.data(), meta::surge_filter_metadata::MESH_POINTS);
                x[0]        = x[2] + 0.5f;
                x[1]        = x[0];
                y[0]        = GAIN_AMP_0_DB;
                y[1]        = y[2];
                x          += meta::surge_filter_metadata::MESH_POINTS + 2;
                y          += meta::surge_filter_metadata::MESH_POINTS + 2;
                x[0]        = x[-1] - 0.5f;
                x[1]        = x[0];
                y[0]        = y[-1];
                y[1]        = GAIN_AMP_0_DB;

                mesh->data(2, meta::surge_filter_metadata::MESH_POINTS + 4);
            }

            // Sync envelope
            mesh    = pMeshEnv->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()) && (bEnvVisible))
            {
                dsp::copy(mesh->pvData[0], vTimePoints, meta::surge_filter_metadata::MESH_POINTS);
                dsp::copy(mesh->pvData[1], sEnv.data(), meta::surge_filter_metadata::MESH_POINTS);
                mesh->data(2, meta::surge_filter_metadata::MESH_POINTS);
            }

            // Sync input mesh
            mesh            = pMeshIn->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                float *x = mesh->pvData[0];

                dsp::copy(&x[1], vTimePoints, meta::surge_filter_metadata::MESH_POINTS);
                x[0]    = x[1];
                x      += meta::surge_filter_metadata::MESH_POINTS + 1;
                x[0]    = x[-1];

                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    float *y        = mesh->pvData[i+1];

                    if (c->bInVisible)
                        dsp::copy(&y[1], c->sIn.data(), meta::surge_filter_metadata::MESH_POINTS);
                    else
                        dsp::fill_zero(&y[1], meta::surge_filter_metadata::MESH_POINTS);
                    y[0]    = 0.0f;
                    y      += meta::surge_filter_metadata::MESH_POINTS + 1;
                    y[0]    = 0.0f;
                }

                mesh->data(nChannels + 1, meta::surge_filter_metadata::MESH_POINTS + 2);
            }

            // Sync output mesh
            mesh            = pMeshOut->buffer<plug::mesh_t>();
            if ((mesh != NULL) && (mesh->isEmpty()))
            {
                dsp::copy(mesh->pvData[0], vTimePoints, meta::surge_filter_metadata::MESH_POINTS);

                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    if (c->bOutVisible)
                        dsp::copy(mesh->pvData[i+1], c->sOut.data(), meta::surge_filter_metadata::MESH_POINTS);
                    else
                        dsp::fill_zero(mesh->pvData[i+1], meta::surge_filter_metadata::MESH_POINTS);
                }

                mesh->data(nChannels + 1, meta::surge_filter_metadata::MESH_POINTS);
            }

            // Query inline display for draw
            bool query_draw = bGainVisible;
            if (!query_draw)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    query_draw      = (c->bInVisible) || (c->bOutVisible);
                    if (query_draw)
                        break;
                }
            }

            if (query_draw)
                pWrapper->query_display_draw();
        }

        bool surge_filter::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > (M_RGOLD_RATIO * width))
                height  = M_RGOLD_RATIO * width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            // Calc axis params
            float zy    = 1.0f/GAIN_AMP_M_144_DB;
            float dx    = -float(width/meta::surge_filter_metadata::MESH_TIME);
            float dy    = height/logf(GAIN_AMP_M_144_DB/GAIN_AMP_P_24_DB);

            // Draw axis
            cv->set_line_width(1.0);

            // Draw vertical lines
            cv->set_color_rgb(CV_YELLOW, 0.5f);
            for (float i=1.0; i < (meta::surge_filter_metadata::MESH_TIME-0.1); i += 1.0f)
            {
                float ax = width + dx*i;
                cv->line(ax, 0, ax, height);
            }

            // Draw horizontal lines
            cv->set_color_rgb(CV_WHITE, 0.5f);
            for (float i=GAIN_AMP_M_144_DB; i<GAIN_AMP_P_24_DB; i *= GAIN_AMP_P_24_DB)
            {
                float ay = height + dy*(logf(i*zy));
                cv->line(0, ay, width, ay);
            }

            // Allocate buffer: t, f1(t), x, y
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            // Draw input signal
            static uint32_t cin_colors[] = {
                    CV_MIDDLE_CHANNEL_IN, CV_MIDDLE_CHANNEL_IN,
                    CV_LEFT_CHANNEL_IN, CV_RIGHT_CHANNEL_IN
                   };
            static uint32_t c_colors[] = {
                    CV_MIDDLE_CHANNEL, CV_MIDDLE_CHANNEL,
                    CV_LEFT_CHANNEL, CV_RIGHT_CHANNEL
                   };
            bool bypass         = vChannels[0].sBypass.bypassing();
            float r             = meta::surge_filter_metadata::MESH_POINTS/float(width);

            for (size_t j=0; j<width; ++j)
            {
                size_t k        = r*j;
                b->v[0][j]      = vTimePoints[k];
            }

            dsp::fill(b->v[2], width, width);
            dsp::fmadd_k3(b->v[2], b->v[0], dx, width);

            // Draw input channels
            cv->set_line_width(2.0f);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                if (!c->bInVisible)
                    continue;

                // Initialize values
                float *ft       = c->sIn.data();
                for (size_t j=0; j<width; ++j)
                    b->v[1][j]      = ft[size_t(r*j)];

                // Initialize coords
                dsp::fill(b->v[3], height, width);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypass) ? CV_SILVER : cin_colors[(nChannels-1)*2 + i]);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            // Draw output channels
            cv->set_line_width(2.0f);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                if (!c->bOutVisible)
                    continue;

                // Initialize values
                float *ft       = c->sOut.data();
                for (size_t j=0; j<width; ++j)
                    b->v[1][j]      = ft[size_t(r*j)];

                // Initialize coords
                dsp::fill(b->v[3], height, width);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypass) ? CV_SILVER : c_colors[(nChannels-1)*2 + i]);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            // Draw envelope (if present)
            if (bEnvVisible)
            {
                float *ft       = sEnv.data();
                for (size_t j=0; j<width; ++j)
                    b->v[1][j]      = ft[size_t(r*j)];

                // Initialize coords
                dsp::fill(b->v[3], height, width);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypass) ? CV_SILVER : CV_BRIGHT_MAGENTA);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            // Draw function (if present)
            if (bGainVisible)
            {
                float *ft       = sGain.data();
                for (size_t j=0; j<width; ++j)
                    b->v[1][j]      = ft[size_t(r*j)];

                // Initialize coords
                dsp::fill(b->v[3], height, width);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width);

                // Draw channel
                cv->set_color_rgb((bypass) ? CV_SILVER : CV_BRIGHT_BLUE);
                cv->draw_lines(b->v[2], b->v[3], width);
            }

            return true;
        }

        void surge_filter::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                const channel_t *c = &vChannels[i];
                v->begin_object(c, sizeof(channel_t));
                {
                    v->write("vIn", c->vIn);
                    v->write("vOut", c->vOut);
                    v->write("vBuffer", c->vBuffer);
                    v->write_object("sBypass", &c->sBypass);
                    v->write_object("sIn", &c->sIn);
                    v->write_object("sOut", &c->sOut);
                    v->write("bInVisible", c->bInVisible);
                    v->write("bOutVisible", c->bOutVisible);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                    v->write("pInVisible", c->pInVisible);
                    v->write("pOutVisible", c->pOutVisible);
                    v->write("pMeterIn", c->pMeterIn);
                    v->write("pMeterOut", c->pMeterOut);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vBuffer", vBuffer);
            v->write("vEnv", vEnv);
            v->write("vTimePoints", vTimePoints);
            v->write("fGainIn", fGainIn);
            v->write("fGainOut", fGainOut);
            v->write("bGainVisible", bGainVisible);
            v->write("bEnvVisible", bEnvVisible);
            v->write("pData", pData);
            v->write("pIDisplay", pIDisplay);

            v->write_object("sGain", &sGain);
            v->write_object("sEnv", &sEnv);
            v->write_object("sActive", &sActive);
            v->write_object("sDepopper", &sDepopper);

            v->write("pModeIn", pModeIn);
            v->write("pModeOut", pModeOut);
            v->write("pGainIn", pGainIn);
            v->write("pGainOut", pGainOut);
            v->write("pThreshOn", pThreshOn);
            v->write("pThreshOff", pThreshOff);
            v->write("pRmsLen", pRmsLen);
            v->write("pFadeIn", pFadeIn);
            v->write("pFadeOut", pFadeOut);
            v->write("pFadeInDelay", pFadeInDelay);
            v->write("pFadeOutDelay", pFadeOutDelay);
            v->write("pActive", pActive);
            v->write("pBypass", pBypass);
            v->write("pMeshIn", pMeshIn);
            v->write("pMeshOut", pMeshOut);
            v->write("pMeshGain", pMeshGain);
            v->write("pMeshEnv", pMeshEnv);
            v->write("pGainVisible", pGainVisible);
            v->write("pEnvVisible", pEnvVisible);
            v->write("pGainMeter", pGainMeter);
            v->write("pEnvMeter", pEnvMeter);
        }
    } /* namespace plugins */
} /* namespace lsp */


