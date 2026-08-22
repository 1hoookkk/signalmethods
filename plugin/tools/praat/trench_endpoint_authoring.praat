# trench_endpoint_authoring.praat
#
# Repository-owned, headless Praat bridge for the native TRENCH Workstation.
# Praat remains an external authoring process; no Praat source is embedded.
#
# Modes:
#   analyze  - full corrected FormantPath plus validation Pitch
#   endpoint - one selected region's pitch-corrected LTAS plus Pitch
#
# The native workstation parses the deterministic text/TSV outputs, computes
# robust endpoint medians, and delegates all root fitting/packing/certification
# to trench-core.

form TRENCH endpoint authoring
    sentence Input_wav
    sentence Out_dir
    sentence Mode
    sentence Endpoint_label
    real Region_start_s -1
    real Region_end_s -1
    positive Time_step_s 0.005
    integer Maximum_number_of_formants 5
    positive Middle_formant_ceiling_Hz 5000
    positive Window_length_s 0.025
    real Pre_emphasis_from_Hz 50
    positive Ceiling_step_size 0.05
    integer Number_of_steps_up_down 4
    real Pitch_time_step_s 0
    positive Pitch_floor_Hz 75
    positive Pitch_ceiling_Hz 600
    positive Ltas_maximum_frequency_Hz 16000
    positive Ltas_bandwidth_Hz 20
    positive Ltas_shortest_period_s 0.0001
    positive Ltas_longest_period_s 0.02
    positive Ltas_maximum_period_factor 1.3
endform

if input_wav$ = "" or not fileReadable (input_wav$)
    exitScript: "cannot read input WAV: " + input_wav$
endif
if out_dir$ = ""
    exitScript: "no output directory"
endif
if mode$ <> "analyze" and mode$ <> "endpoint"
    exitScript: "mode must be analyze or endpoint"
endif
if maximum_number_of_formants <> 5
    exitScript: "this endpoint workflow requires exactly five FormantPath lanes"
endif
if time_step_s <= 0 or window_length_s <= 0
    exitScript: "invalid FormantPath time/window settings"
endif
if pitch_floor_Hz <= 0 or pitch_ceiling_Hz <= pitch_floor_Hz
    exitScript: "invalid pitch floor/ceiling"
endif

createFolder: out_dir$
sound = Read from file: input_wav$
sourceSampleRate = Get sampling frequency
sourceChannels = Get number of channels
sourceDuration = Get total duration
if sourceDuration <= 0
    removeObject: sound
    exitScript: "input WAV has no duration"
endif

if sourceChannels > 1
    selectObject: sound
    mono = Convert to mono
    analysisSound = mono
else
    mono = 0
    analysisSound = sound
endif

if mode$ = "analyze"
    selectObject: analysisSound
    path = To FormantPath (burg): time_step_s, maximum_number_of_formants,
        ... middle_formant_ceiling_Hz, window_length_s, pre_emphasis_from_Hz,
        ... ceiling_step_size, number_of_steps_up_down
    selectObject: path
    candidateCount = Get number of candidates
    if candidateCount < 1
        exitScript: "FormantPath has no candidates"
    endif
    # Use Praat's native automatic candidate correction. These are the stable
    # defaults of FormantPath: Path finder; they are recorded below so the
    # selected trajectory is fully reproducible.
    Path finder: 1.0, 1.0, 1.0, 1.0, 5.0, 0.035, "3 3 3 3", 1.25
    Save as text file: out_dir$ + "/trajectory.FormantPath.txt"

    selectObject: analysisSound
    pitch = To Pitch: pitch_time_step_s, pitch_floor_Hz, pitch_ceiling_Hz
    selectObject: pitch
    Save as text file: out_dir$ + "/pitch_full.Pitch.txt"
    pitchTsv$ = out_dir$ + "/pitch_full.tsv"
    writeFileLine: pitchTsv$, "frame", tab$, "time_s", tab$, "f0_Hz"
    pitchFrames = Get number of frames
    for frame to pitchFrames
        time = Get time from frame number: frame
        f0 = Get value in frame: frame, "Hertz"
        appendFileLine: pitchTsv$, string$ (frame), tab$, fixed$ (time, 9),
            ... tab$, if f0 = undefined then "undefined" else fixed$ (f0, 6) fi
    endfor

    manifest$ = out_dir$ + "/analysis_settings.tsv"
    writeFileLine: manifest$, "format_version", tab$, "2"
    appendFileLine: manifest$, "praat_version", tab$, praatVersion$
    appendFileLine: manifest$, "mode", tab$, "analyze"
    appendFileLine: manifest$, "source_sample_rate_hz", tab$, fixed$ (sourceSampleRate, 6)
    appendFileLine: manifest$, "source_channels", tab$, string$ (sourceChannels)
    appendFileLine: manifest$, "source_duration_s", tab$, fixed$ (sourceDuration, 9)
    appendFileLine: manifest$, "analysis_channels", tab$, "1"
    appendFileLine: manifest$, "time_step_s", tab$, fixed$ (time_step_s, 9)
    appendFileLine: manifest$, "maximum_number_of_formants", tab$, string$ (maximum_number_of_formants)
    appendFileLine: manifest$, "middle_formant_ceiling_hz", tab$, fixed$ (middle_formant_ceiling_Hz, 6)
    appendFileLine: manifest$, "window_length_s", tab$, fixed$ (window_length_s, 9)
    appendFileLine: manifest$, "pre_emphasis_from_hz", tab$, fixed$ (pre_emphasis_from_Hz, 6)
    appendFileLine: manifest$, "ceiling_step_size", tab$, fixed$ (ceiling_step_size, 9)
    appendFileLine: manifest$, "number_of_steps_up_down", tab$, string$ (number_of_steps_up_down)
    appendFileLine: manifest$, "candidate_count", tab$, string$ (candidateCount)
    appendFileLine: manifest$, "candidate_selection", tab$, "FormantPath: Path finder"
    appendFileLine: manifest$, "path_finder_f_over_b_weight", tab$, "1.0"
    appendFileLine: manifest$, "path_finder_frequency_change_weight", tab$, "1.0"
    appendFileLine: manifest$, "path_finder_stress_weight", tab$, "1.0"
    appendFileLine: manifest$, "path_finder_ceiling_change_weight", tab$, "1.0"
    appendFileLine: manifest$, "path_finder_intensity_step_db", tab$, "5.0"
    appendFileLine: manifest$, "path_finder_window_length_s", tab$, "0.035"
    appendFileLine: manifest$, "path_finder_coefficients_by_track", tab$, "3 3 3 3"
    appendFileLine: manifest$, "path_finder_power", tab$, "1.25"
    for candidate to candidateCount
        ceiling = middle_formant_ceiling_Hz * exp (
            ... (candidate - number_of_steps_up_down - 1) * ceiling_step_size)
        appendFileLine: manifest$, "candidate_", string$ (candidate), "_ceiling_hz",
            ... tab$, fixed$ (ceiling, 6)
    endfor
    appendFileLine: manifest$, "pitch_time_step_s", tab$, fixed$ (pitch_time_step_s, 9)
    appendFileLine: manifest$, "pitch_floor_hz", tab$, fixed$ (pitch_floor_Hz, 6)
    appendFileLine: manifest$, "pitch_ceiling_hz", tab$, fixed$ (pitch_ceiling_Hz, 6)

    removeObject: pitch
    removeObject: path
else
    if endpoint_label$ <> "AY" and endpoint_label$ <> "EE"
        exitScript: "endpoint label must be AY or EE"
    endif
    if region_start_s < 0 or region_end_s <= region_start_s or region_end_s > sourceDuration
        exitScript: "endpoint interval is outside the WAV"
    endif
    selectObject: analysisSound
    region = Extract part: region_start_s, region_end_s, "rectangular", 1, "no"

    selectObject: region
    pitch = To Pitch: pitch_time_step_s, pitch_floor_Hz, pitch_ceiling_Hz
    pitchTsv$ = out_dir$ + "/pitch_" + endpoint_label$ + ".tsv"
    writeFileLine: pitchTsv$, "frame", tab$, "time_s", tab$, "f0_Hz"
    selectObject: pitch
    pitchFrames = Get number of frames
    for frame to pitchFrames
        time = Get time from frame number: frame
        f0 = Get value in frame: frame, "Hertz"
        appendFileLine: pitchTsv$, string$ (frame), tab$,
            ... fixed$ (time + region_start_s, 9), tab$,
            ... if f0 = undefined then "undefined" else fixed$ (f0, 6) fi
    endfor

    selectObject: region
    ltas = To Ltas (pitch-corrected): pitch_floor_Hz, pitch_ceiling_Hz,
        ... ltas_maximum_frequency_Hz, ltas_bandwidth_Hz,
        ... ltas_shortest_period_s, ltas_longest_period_s,
        ... ltas_maximum_period_factor
    ltasTsv$ = out_dir$ + "/ltas_" + endpoint_label$ + ".tsv"
    writeFileLine: ltasTsv$, "freq_Hz", tab$, "dB"
    selectObject: ltas
    bins = Get number of bins
    validBins = 0
    for bin to bins
        frequency = Get frequency from bin number: bin
        value = Get value in bin: bin
        if value <> undefined
            appendFileLine: ltasTsv$, fixed$ (frequency, 6), tab$, fixed$ (value, 9)
            validBins = validBins + 1
        endif
    endfor
    if validBins < 2
        exitScript: "pitch-corrected LTAS produced fewer than two valid bins"
    endif

    settings$ = out_dir$ + "/endpoint_" + endpoint_label$ + "_settings.tsv"
    writeFileLine: settings$, "format_version", tab$, "2"
    appendFileLine: settings$, "praat_version", tab$, praatVersion$
    appendFileLine: settings$, "mode", tab$, "endpoint"
    appendFileLine: settings$, "endpoint", tab$, endpoint_label$
    appendFileLine: settings$, "region_start_s", tab$, fixed$ (region_start_s, 9)
    appendFileLine: settings$, "region_end_s", tab$, fixed$ (region_end_s, 9)
    appendFileLine: settings$, "pitch_floor_hz", tab$, fixed$ (pitch_floor_Hz, 6)
    appendFileLine: settings$, "pitch_ceiling_hz", tab$, fixed$ (pitch_ceiling_Hz, 6)
    appendFileLine: settings$, "ltas_operation", tab$, "Sound: To Ltas (pitch-corrected)"
    appendFileLine: settings$, "ltas_maximum_frequency_hz", tab$, fixed$ (ltas_maximum_frequency_Hz, 6)
    appendFileLine: settings$, "ltas_bandwidth_hz", tab$, fixed$ (ltas_bandwidth_Hz, 6)
    appendFileLine: settings$, "ltas_shortest_period_s", tab$, fixed$ (ltas_shortest_period_s, 9)
    appendFileLine: settings$, "ltas_longest_period_s", tab$, fixed$ (ltas_longest_period_s, 9)
    appendFileLine: settings$, "ltas_maximum_period_factor", tab$, fixed$ (ltas_maximum_period_factor, 9)
    appendFileLine: settings$, "valid_bins", tab$, string$ (validBins)

    removeObject: ltas
    removeObject: pitch
    removeObject: region
endif

if mono <> 0
    removeObject: mono
endif
removeObject: sound
writeInfoLine: "TRENCH Praat ", mode$, " complete"
