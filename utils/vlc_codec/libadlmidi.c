/*****************************************************************************
 * libadlmidi.c: Software MIDI synthesizer using OPL3 Synth emulation
 *****************************************************************************
 * Copyright © 2015-2025 Vitaly Novichkov
 * $Id$
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_dialog.h>
#include <vlc/libvlc_version.h>

#include <unistd.h>

#ifndef N_
#define N_(x) (x)
#endif

#ifndef _
#define _(x) (x)
#endif

#include <adlmidi.h>

#define ENABLE_CODEC_TEXT N_("Enable this synthesizer")
#define ENABLE_CODEC_LONGTEXT N_( \
    "Enable using of this synthesizer, otherwise, other MIDI synthesizers will be used")

#define FMBANK_TEXT N_("Custom bank file")
#define FMBANK_LONGTEXT N_( \
    "Custom bank file (in WOPL format) to use for software synthesis." )

#if 0 /* Old code */
#define CHORUS_TEXT N_("Chorus")

#define GAIN_TEXT N_("Synthesis gain")
#define GAIN_LONGTEXT N_("This gain is applied to synthesis output. " \
    "High values may cause saturation when many notes are played at a time." )

#define POLYPHONY_TEXT N_("Polyphony")
#define POLYPHONY_LONGTEXT N_( \
    "The polyphony defines how many voices can be played at a time. " \
    "Larger values require more processing power.")

#define REVERB_TEXT N_("Reverb")
#endif

#define SAMPLE_RATE_TEXT N_("Sample rate")

#define EMULATED_CHIPS_TEXT N_("Count of emulated chips")
#define EMULATED_CHIPS_LONGTEXT N_( \
    "How many emulated chips will be processed to expand channels limit of a single chip." )

#define EMBEDDED_BANK_ID_TEXT N_("Embedded bank")
#define EMBEDDED_BANK_ID_LONGTEXT N_( \
    "Use one of embedded banks.")

#define FULL_PANNING_TEXT N_("Full panning")
#define FULL_PANNING_LONGTEXT N_( \
    "Enable full-panning stereo support")

#define VOLUME_MODEL_TEXT N_("Volume scaling model")
#define VOLUME_MODEL_LONGTEXT N_( \
    "Declares volume scaling model which will affect volume levels.")

#define CHANNEL_ALLOCATION_TEXT N_("Channel allocation mode")
#define CHANNEL_ALLOCATION_LONGTEXT N_( \
    "Declares the method of chip channel allocation for new notes.")

#define FULL_RANGE_CC74_TEXT N_("Full-range of brightness")
#define FULL_RANGE_CC74_LONGTEXT N_( \
    "Scale range of CC-74 \"Brightness\" with full 0~127 range. By default is only 0~64 affects the sounding.")

#define ENABLE_AUTO_ARPEGGIO_TEXT N_("Enable auto-arpeggio")
#define ENABLE_AUTO_ARPEGGIO_LONGTEXT N_( \
    "Enables an automatical arpeggio to keep chords playing when there is no enough free physical voices of chips.")


#ifdef ADLMIDI_ENABLE_HW_SERIAL

#define SERIAL_ENABLE_TEXT N_("Enable Serial hardware mode [EXPERIMENTAL]")
#define SERIAL_ENABLE_LONGTEXT N_( \
    "Use the hardware OPL3 chip via Serial port.\n\nNote: In the VLC this feature is EXPERIMENTAL and has some deffects like a short tempo boost at begining.")

#define SERIAL_NAME_TEXT N_("Serial device name")
#if defined(_WIN32)
#   define SERIAL_NAME_LONGTEXT N_( "Name of the Serial device to use. (For example, COM1, COM2, COM3, etc.)")
#elif defined(__APPLE__)
#   define SERIAL_NAME_LONGTEXT N_( "Name of the Serial device to use. (For example, \"cu.usbmodem2401\", etc.)")
#else
#   define SERIAL_NAME_LONGTEXT N_( "Name of the Serial device to use. (For example, ttyACM0, ttyACM1, etc.)")
#endif

#define SERIAL_BAUD_TEXT N_("Serial baud speed")
#define SERIAL_BAUD_LONGTEXT N_( \
    "The baud speed value of the Serial device.")

#define SERIAL_PROTO_TEXT N_("Serial protocol")
#define SERIAL_PROTO_LONGTEXT N_( \
    "The protocol to use the Serial device.")

static const int serial_protos_values[] = { 1, 2, 3 };
static const char * const serial_protos_descriptions[] =
    {
        N_("Arduino OPL2"),
        N_("NukeYkt OPL3"),
        N_("RetroWave OPL3"),
        NULL
};
#endif // ADLMIDI_ENABLE_HW_SERIAL


static const int volume_models_values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const char * const volume_models_descriptions[] =
{
    N_("Auto (defined by bank)"),
    N_("Generic"),
    N_("OPL3 Native"),
    N_("DMX"),
    N_("Apogee Sound System"),
    N_("Win9x SB16 driver"),
    N_("DMX (Fixed AM)"),
    N_("Apogee Sound System (Fixed AM)"),
    N_("Audio Interfaces Library (AIL)"),
    N_("Win9x Generic FM driver"),
    N_("HMI Sound Operating System"),
    N_("HMI Sound Operating System (Old)"),
    NULL
};

static const int channel_alloc_values[] = { -1, 0, 1, 2 };
static const char * const channel_alloc_descriptions[] =
{
    N_("Auto (defined by bank)"),
    N_("By sounding delays"),
    N_("Released channel of same instrument"),
    N_("Any released channel"),
    NULL
};

#define EMULATOR_TYPE_TEXT N_("OPL3 Emulation core")
#define EMULATOR_TYPE_LINGTEXT N_( \
    "OPL3 Emulator that will be used to generate final sound.")

static const int emulator_type_values[] =
{
#ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
    (int)ADLMIDI_EMU_NUKED,
    (int)ADLMIDI_EMU_NUKED_174,
#endif

#ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
    (int)ADLMIDI_EMU_DOSBOX,
#endif

#ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
    (int)ADLMIDI_EMU_OPAL,
#endif

#ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
    (int)ADLMIDI_EMU_JAVA,
#endif

#ifndef ADLMIDI_DISABLE_ESFMU_EMULATOR
    (int)ADLMIDI_EMU_ESFMu,
#endif

#ifndef ADLMIDI_DISABLE_MAME_OPL2_EMULATOR
    (int)ADLMIDI_EMU_MAME_OPL2,
#endif

#ifndef ADLMIDI_DISABLE_YMFM_EMULATOR
    (int)ADLMIDI_EMU_YMFM_OPL2,
    (int)ADLMIDI_EMU_YMFM_OPL3,
#endif

#ifdef ADLMIDI_ENABLE_OPL2_LLE_EMULATOR
    (int)ADLMIDI_EMU_NUKED_OPL2_LLE,
#endif

#ifdef ADLMIDI_ENABLE_OPL3_LLE_EMULATOR
    (int)ADLMIDI_EMU_NUKED_OPL3_LLE,
#endif
};
static const char * const emulator_type_descriptions[] =
{
#ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
    N_("Nuked OPL3 1.8"),
    N_("Nuked OPL3 1.7.4 (Optimized)"),
#endif

#ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
    N_("DOSBox"),
#endif

#ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
    N_("Opal"),
#endif

#ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
    N_("Java OPL3"),
#endif

#ifndef ADLMIDI_DISABLE_ESFMU_EMULATOR
    N_("ESFMu"),
#endif

#ifndef ADLMIDI_DISABLE_MAME_OPL2_EMULATOR
    N_("MAME OPL2"),
#endif

#ifndef ADLMIDI_DISABLE_YMFM_EMULATOR
    N_("YMFM OPL2"),
    N_("YMFM OPL3"),
#endif

#ifdef ADLMIDI_ENABLE_OPL2_LLE_EMULATOR
    N_("Nuked OPL2-LLE [!EXTRA HEAVY!]"),
#endif

#ifdef ADLMIDI_ENABLE_OPL3_LLE_EMULATOR
    N_("Nuked OPL3-LLE [!EXTRA HEAVY!]"),
#endif

    NULL
};

static int embedded_bank_count = 0, embedded_bank_i = 0;
static int embedded_bank_values[100];

static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

#define CONFIG_PREFIX "adlmidi-"

vlc_module_begin ()
    set_description (N_("ADLMIDI OPL3 Synth MIDI synthesizer"))
#if (LIBVLC_VERSION_MAJOR >= 3)
    set_capability ("audio decoder", 150)
#else
    set_capability ("decoder", 150)
#endif
    set_shortname (N_("ADLMIDI"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACODEC)
    set_callbacks (Open, Close)

    add_bool( CONFIG_PREFIX "enable", true, ENABLE_CODEC_TEXT,
              ENABLE_CODEC_LONGTEXT, false )

    embedded_bank_count = adl_getBanksCount();
    for (embedded_bank_i = 0; embedded_bank_i < embedded_bank_count; embedded_bank_i++)
        embedded_bank_values[embedded_bank_i] = embedded_bank_i;
    add_integer (CONFIG_PREFIX "internal-bank-id", 58, EMBEDDED_BANK_ID_TEXT, EMBEDDED_BANK_ID_LONGTEXT, true)
        /* change_integer_list( embedded_bank_values, embedded_bank_descriptions ) */
        vlc_config_set (VLC_CONFIG_LIST,
                        (size_t)(adl_getBanksCount()),
                        (const int *)(embedded_bank_values),
                        (const char *const *)(adl_getBankNames()));

    add_loadfile (CONFIG_PREFIX "custombank", "",
                  FMBANK_TEXT, FMBANK_LONGTEXT, false)

    add_integer (CONFIG_PREFIX "volume-model", 0, VOLUME_MODEL_TEXT, VOLUME_MODEL_LONGTEXT, false )
        change_integer_list( volume_models_values, volume_models_descriptions )

    add_integer (CONFIG_PREFIX "channel-allocation", -1, CHANNEL_ALLOCATION_TEXT, CHANNEL_ALLOCATION_LONGTEXT, false )
        change_integer_list( channel_alloc_values, channel_alloc_descriptions )

    add_integer (CONFIG_PREFIX "emulator-type", 0, EMULATOR_TYPE_TEXT, EMULATOR_TYPE_LINGTEXT, false)
        change_integer_list( emulator_type_values, emulator_type_descriptions )

    add_integer (CONFIG_PREFIX "emulated-chips", 6, EMULATED_CHIPS_TEXT, EMULATED_CHIPS_LONGTEXT, true)
        change_integer_range (1, 100)

    add_integer (CONFIG_PREFIX "sample-rate", 44100, SAMPLE_RATE_TEXT, SAMPLE_RATE_TEXT, true)
        change_integer_range (22050, 96000)

    add_bool( CONFIG_PREFIX "full-range-brightness", false, FULL_RANGE_CC74_TEXT,
              FULL_RANGE_CC74_LONGTEXT, true )

    add_bool( CONFIG_PREFIX "enable-auto-arpeggio", true, ENABLE_AUTO_ARPEGGIO_TEXT,
              ENABLE_AUTO_ARPEGGIO_LONGTEXT, true )

    add_bool( CONFIG_PREFIX "full-panning", true, FULL_PANNING_TEXT,
              FULL_PANNING_LONGTEXT, true )

#ifdef ADLMIDI_ENABLE_HW_SERIAL
    add_bool(CONFIG_PREFIX "serial-enable", false, SERIAL_ENABLE_TEXT, SERIAL_ENABLE_LONGTEXT, true)
    add_string(CONFIG_PREFIX "serial-name", "", SERIAL_NAME_TEXT, SERIAL_NAME_LONGTEXT, true)
    add_integer(CONFIG_PREFIX "serial-baud", 115200, SERIAL_BAUD_TEXT, SERIAL_BAUD_LONGTEXT, true)
        change_integer_range (9600, 115200)

    add_integer (CONFIG_PREFIX "serial-proto", 3, SERIAL_PROTO_TEXT, SERIAL_PROTO_LONGTEXT, false)
        change_integer_list( serial_protos_values, serial_protos_descriptions )
#endif


vlc_module_end ()


struct decoder_sys_t
{
    struct ADL_MIDIPlayer *synth;
    int               sample_rate;
    int               soundfont;
    date_t            end_date;
    int               is_serial;
};

static const struct ADLMIDI_AudioFormat g_output_format =
{
    ADLMIDI_SampleType_F32,
    sizeof(float),
    2 * sizeof(float)
};

#if (LIBVLC_VERSION_MAJOR >= 3)
static int DecodeBlock (decoder_t *p_dec, block_t *p_block);
#else
static block_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block);
#endif
static void Flush (decoder_t *);

static int Open (vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_MIDI)
        return VLC_EGENERIC;

    if (!var_InheritBool(p_this, CONFIG_PREFIX "enable"))
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->sample_rate = var_InheritInteger (p_this, CONFIG_PREFIX "sample-rate");
    p_sys->synth = adl_init( p_sys->sample_rate );

    if(var_InheritBool(p_this, CONFIG_PREFIX "serial-enable"))
    {
        adl_switchSerialHW(p_sys->synth,
            var_InheritString(p_this, CONFIG_PREFIX "serial-name"),
            var_InheritInteger(p_this, CONFIG_PREFIX "serial-baud"),
            var_InheritInteger(p_this, CONFIG_PREFIX "serial-proto")
        );
        p_sys->is_serial = 1;
    }
    else
    {
        adl_switchEmulator(p_sys->synth, var_InheritInteger(p_this, CONFIG_PREFIX "emulator-type"));
        adl_setNumChips(p_sys->synth, var_InheritInteger(p_this, CONFIG_PREFIX "emulated-chips"));
        adl_setSoftPanEnabled(p_sys->synth, var_InheritBool(p_this, CONFIG_PREFIX "full-panning"));
        p_sys->is_serial = 0;
    }

    adl_setVolumeRangeModel(p_sys->synth, var_InheritInteger(p_this, CONFIG_PREFIX "volume-model"));
    adl_setChannelAllocMode(p_sys->synth, var_InheritInteger(p_this, CONFIG_PREFIX "channel-allocation"));

    adl_setFullRangeBrightness(p_sys->synth, var_InheritBool(p_this, CONFIG_PREFIX "full-range-brightness"));

    adl_setAutoArpeggio(p_sys->synth, var_InheritBool(p_this, CONFIG_PREFIX "enable-auto-arpeggio"));

    char *bank_path = var_InheritString (p_this, CONFIG_PREFIX "custombank");
    if (bank_path != NULL)
    {
        msg_Dbg (p_this, "loading custom bank file %s", bank_path);
        if (adl_openBankFile(p_sys->synth, bank_path))
        {
            msg_Warn (p_this, "cannot load custom bank file %s: %s", bank_path, adl_errorInfo(p_sys->synth));
            adl_setBank(p_sys->synth, var_InheritInteger(p_this, CONFIG_PREFIX "internal-bank-id"));
        }
        free (bank_path);
    }
    else
    {
        adl_setBank(p_sys->synth, var_InheritInteger(p_this, CONFIG_PREFIX "internal-bank-id"));
    }

    p_dec->fmt_out.i_cat = AUDIO_ES;

    p_dec->fmt_out.audio.i_rate = p_sys->sample_rate;
    p_dec->fmt_out.audio.i_channels = 2;
    p_dec->fmt_out.audio.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;

    p_dec->fmt_out.i_codec = VLC_CODEC_F32L;
    p_dec->fmt_out.audio.i_bitspersample = 32;
    date_Init (&p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1);
    date_Set (&p_sys->end_date, 0);

    p_dec->p_sys = p_sys;
#if (LIBVLC_VERSION_MAJOR >= 3)
    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush = Flush;
#else
    p_dec->pf_decode_audio = DecodeBlock;
#endif
    return VLC_SUCCESS;
}


static void Close (vlc_object_t *p_this)
{
    decoder_sys_t *p_sys = ((decoder_t *)p_this)->p_sys;

    if(p_sys->is_serial)
        adl_panic(p_sys->synth);

    adl_close(p_sys->synth);
    free (p_sys);
}

static void Flush (decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

#if (LIBVLC_VERSION_MAJOR >= 3)
    date_Set (&p_sys->end_date, VLC_TS_INVALID);
#else
    date_Set (&p_sys->end_date, 0);
#endif
    adl_panic(p_sys->synth);
}


#if (LIBVLC_VERSION_MAJOR >= 3)
static int DecodeBlock (decoder_t *p_dec, block_t *p_block)
{
    size_t it;
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_out = NULL;

#else
static block_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block)
{
    size_t it;
    block_t *p_block;
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_out = NULL;
    if (pp_block == NULL)
        return NULL;
    p_block = *pp_block;
#endif

    if (p_block == NULL)
    {
#if (LIBVLC_VERSION_MAJOR >= 3)
        return VLCDEC_SUCCESS;
#else
        return p_out;
#endif
    }

#if (LIBVLC_VERSION_MAJOR < 3)
    *pp_block = NULL;
#endif

    if (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
    {
#if (LIBVLC_VERSION_MAJOR >= 3)
        Flush (p_dec);
        if (p_block->i_flags & BLOCK_FLAG_CORRUPTED)
        {
            block_Release(p_block);
            return VLCDEC_SUCCESS;
        }
#else
        Flush (p_dec);
#endif
    }

    if (p_block->i_pts > VLC_TS_INVALID
        && !date_Get (&p_sys->end_date))
        date_Set (&p_sys->end_date, p_block->i_pts);
    else
    if (p_block->i_pts < date_Get (&p_sys->end_date))
    {
        msg_Warn (p_dec, "MIDI message in the past?");
        goto drop;
    }

    if (p_block->i_buffer < 1)
        goto drop;

    uint8_t event = p_block->p_buffer[0];
    uint8_t channel = p_block->p_buffer[0] & 0xf;
    event &= 0xF0;

    if (event == 0xF0)
        switch (channel)
        {
            case 0:
                if (p_block->p_buffer[p_block->i_buffer - 1] != 0xF7)
                {
            case 7:
                    msg_Warn (p_dec, "fragmented SysEx not implemented");
                    goto drop;
                }
                adl_rt_systemExclusive(p_sys->synth,
                                       (const ADL_UInt8 *)p_block->p_buffer,
                                       p_block->i_buffer);
                break;
            case 0xF:
                adl_rt_resetState(p_sys->synth);
                break;
        }

    uint8_t p1 = (p_block->i_buffer > 1) ? (p_block->p_buffer[1] & 0x7f) : 0;
    uint8_t p2 = (p_block->i_buffer > 2) ? (p_block->p_buffer[2] & 0x7f) : 0;

    switch (event & 0xF0)
    {
        case 0x80:
            adl_rt_noteOff(p_sys->synth, channel, p1);
            break;
        case 0x90:
            adl_rt_noteOn(p_sys->synth, channel, p1, p2);
            break;
        case 0xA0:
            adl_rt_noteAfterTouch(p_sys->synth, channel, p1, p2);
            break;
        case 0xB0:
            adl_rt_controllerChange(p_sys->synth, channel, p1, p2);
            break;
        case 0xC0:
            adl_rt_patchChange(p_sys->synth, channel, p1);
            break;
        case 0xD0:
            adl_rt_channelAfterTouch(p_sys->synth, channel, p1);
            break;
        case 0xE0:
            adl_rt_pitchBendML(p_sys->synth, channel, p2, p1);
            break;
    }

    unsigned samples = (p_block->i_pts - date_Get (&p_sys->end_date)) * 441 / 10000;
#ifdef ADLMIDI_ENABLE_HW_SERIAL
    double delay = ((p_block->i_pts - date_Get (&p_sys->end_date)) / 1000000.0) / 2.0;
#endif

    if (samples == 0)
        goto drop;

#if (LIBVLC_VERSION_MAJOR >= 3)
    if (decoder_UpdateAudioFormat (p_dec))
        goto drop;
#endif

    p_out = decoder_NewAudioBuffer (p_dec, samples);
    if (p_out == NULL)
        goto drop;

    p_out->i_pts = date_Get (&p_sys->end_date );

#ifdef ADLMIDI_ENABLE_HW_SERIAL
    if(p_sys->is_serial)
    {
        if(delay > 0.0)
            adl_tickIterators(p_sys->synth, delay);

        for (it = 0; it < samples * 2; ++it)
            ((float*)p_out->p_buffer)[it] = 0.0f;
    }
    else
#endif
    {
        samples = adl_generateFormat(p_sys->synth, (int)samples * 2,
                                     (ADL_UInt8*)p_out->p_buffer,
                                     (ADL_UInt8*)(p_out->p_buffer + g_output_format.containerSize),
                                     &g_output_format);

        for (it = 0; it < samples; ++it)
            ((float*)p_out->p_buffer)[it] *= 2.0f;
    }

    samples /= 2;
    p_out->i_length = date_Increment (&p_sys->end_date, samples) - p_out->i_pts;

drop:
    block_Release (p_block);
#if (LIBVLC_VERSION_MAJOR >= 3)
    if (p_out != NULL)
        decoder_QueueAudio (p_dec, p_out);
    return VLCDEC_SUCCESS;
#else
    return p_out;
#endif
}

