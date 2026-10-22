# Reference sources — checked 2026-10-22

- Spotify: https://support.spotify.com/us/artists/article/loudness-normalization/
  Normal -14 LUFS; headroom recommendations -1 dBTP or -2 for louder masters. This is not universal delivery pass/fail.
- Dolby: https://professionalsupport.dolby.com/s/article/Measuring-the-Loudness-of-a-Dolby-Atmos-Mix?language=en_US
  Music reference -18 LKFS / -1 dBTP. Use Renderer or its 5.1 loudness re-render for third-party loudness plugins.
- Apple Digital Masters: https://www.apple.com/cl/apple-music/apple-digital-masters/docs/apple-digital-masters.pdf
  Sound Check adjusts playback loudness. The -16 preset supplied here is only a developer planning reference, not a verified Apple stereo delivery rule.
- YouTube: no official fixed LUFS delivery requirement was established in this research. -14/-1 is a developer planning reference.
- libebur128: https://github.com/jiixyj/libebur128/tree/v1.2.6 (MIT)
- Channel weighting: https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BS.2217-2-2016-PDF-E.pdf
- libfvad: https://github.com/dpirch/libfvad/tree/v1.0 (BSD-style; see LICENSE and PATENTS)
  Zero-masked VAD-selected loudness is an approximation, not Dolby proprietary dialogue gating.
- JUCE: https://github.com/juce-framework/JUCE/tree/8.0.16-security
  Review applicable licence before distribution; no commercial licence entitlement supplied.
- GitHub actions: https://github.com/actions/checkout/releases/tag/v7.0.1
  https://github.com/actions/upload-artifact/releases/tag/v7.0.2

Logo supplied by user. No Dolby certification marks used.
