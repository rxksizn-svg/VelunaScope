# Veluna Scope 0.1 — คู่มือภาษาไทย

สถานะ: ซอร์สต้นแบบ Windows x64 VST3 ยังไม่ได้คอมไพล์ รันชุดทดสอบ C++ หรือทดสอบใน DAW ในแชตนี้ ผล build ของ Bloom/Halo ไม่ใช่ผล build ของ Scope

## มิเตอร์
- Momentary: loudness หน้าต่าง 400 ms
- Short-term: loudness หน้าต่าง 3 วินาที
- Integrated (level-gated): loudness สะสมพร้อม absolute/relative gating
- Loudness Range: EBU LRA หน่วย LU ควรใช้ข้อมูลยาวพอเป็นตัวแทนทั้งเพลง
- True Peak Max: พีกสูงสุดสะสม พร้อมพีกแยกแชนเนล LFE ไม่นับใน loudness แต่นับใน peak
- Speech-gated (VAD estimate): ค่าประเมินจาก WebRTC VAD ไม่ใช่ Dolby Dialogue Intelligence
- % VAD-selected audio: สัดส่วน frame ที่ตัวตรวจเลือก ไม่ใช่เปอร์เซ็นต์คำพูด/เสียงร้องที่รับรองความแม่นยำ

VAD วิเคราะห์ Center เมื่อมี Center มิฉะนั้นใช้ mono หรือค่าเฉลี่ย L+R ผ่าน low-pass/resampling เฉพาะทางตรวจเสียงพูด จากนั้น zero-mask ช่วง 20ms ที่ไม่เลือกก่อนวัด integrated อีกชุด วิธีนี้ไม่เท่ากับมาตรฐาน dialogue-gated ของ Dolby และอาจตรวจเพลงเป็นคำพูดหรือพลาดคำพูดในดนตรี ใช้ประกอบการทดลองเท่านั้น

## รูปแบบเสียง
รองรับ matching mono/stereo/5.1/7.1/7.1.4 PCM ไม่บังคับขยายสเตอริโอเป็น 12 ช่อง โหมดจริงถูกกำหนดจาก bus layout ของ host แสดงชื่อแชนเนลที่ host ส่งให้ และจับคู่น้ำหนักผ่านชนิดแชนเนล JUCE/BS.1770 directional enums
ให้ทดสอบ tone ทีละช่องเพื่อตรวจ routing ใน DAW
ไม่มี Atmos object decoder, ADM BWF reader, binaural renderer, downmix renderer หรือการตรวจ metadata

## Dolby Atmos Music
ใช้ Renderer หรือวางมิเตอร์บน **5.1 loudness re-render จาก Dolby Atmos Renderer** ตามคำแนะนำ Dolby ไม่ใช่ทำ downmix 5.1 เองหรืออ่าน 7.1.4 monitor bus แล้วรับรอง Atmos
ตัวเลือก Dolby เปรียบเทียบกับ -18 LUFS / -1 dBTP หากไม่ใช่ 6 ช่องจะแจ้งเตือน แม้เป็น 6 ช่องก็ไม่สามารถพิสูจน์จากเสียงอย่างเดียวได้ว่ามาจาก loudness re-render จริง
7.1.4 PCM monitoring ไม่ใช่การวัด object-based Atmos master ใช้เครื่องมือที่ผู้รับงานยอมรับตรวจงานขั้นสุดท้าย

## Reference profiles
| ตัวเลือก | LUFS | Peak | สถานะ |
|---|---:|---:|---|
| Spotify Normal | -14 | -1 dBTP; -2 เมื่อ integrated > -14 | ข้อมูล normalize/คำแนะนำ Spotify |
| YouTube custom reference | -14 | -1 dBTP | ค่าเริ่มต้นวางแผนโดยผู้พัฒนา ไม่ใช่ข้อกำหนดทางการที่ยืนยันแล้ว |
| Apple Music custom reference | -16 | -1 dBTP | ค่าเริ่มต้นวางแผน ไม่ใช่ข้อกำหนด stereo ที่ Apple รับรอง |
| Dolby Atmos Music guide | -18 | -1 dBTP | เทียบอ้างอิงบน Renderer 5.1 loudness re-render |
| Custom | ปรับได้ | ปรับได้ | ใส่ข้อกำหนดของผู้รับงานเอง |

Target difference เป็น reference ลบ integrated หน่วย dB ไม่ใช่การทำนาย playback normalization จริงในทุกโหมด และไม่ใช่ผลผ่าน/ไม่ผ่านคุณภาพของมาสเตอร์ ปลั๊กอินไม่ปรับ gain และไม่ทดสอบ codec clipping
หากต้องการเปลี่ยนค่าเริ่มต้น YouTube/Apple ให้เลือก Custom แล้วใส่ค่าที่ต้องการ

## Build ออนไลน์
โปรเจกต์นี้แยกจาก Bloom/Halo อย่าทับ CMakeLists.txt ของโปรเจกต์เดิม
1. สร้าง private repository ใหม่ เช่น veluna-scope
2. แตก ZIP อัปโหลดไฟล์ภายใน VelunaScope ไป root ให้เห็น CMakeLists.txt, Source, Tests, Assets, scripts และ .github
3. ถ้า .github ไม่ถูกอัปโหลด ให้สร้าง .github/workflows/scope-windows.yml แล้วคัดลอกเนื้อหาจากไฟล์ที่ให้
4. Actions > Build Veluna Scope for Windows > Run workflow หรือ push เข้า main
5. รอ build และ measurement checks ผ่าน จากนั้นดาวน์โหลด artifact VelunaScope-Windows-x64
6. แตก artifact ZIP และ ZIP ข้างใน คัดลอกทั้งโฟลเดอร์ Veluna Scope.vst3 ไป C:\Program Files\Common Files\VST3 แล้ว rescan ใน DAW
7. ถ้าล้มเหลว ส่งข้อความ error แรกจากขั้นตอนสีแดงให้ตรวจ
ใช้โควตา Actions ของบัญชี ไม่มีการเปลี่ยนงบหรือสร้าง public release โดยอัตโนมัติ

## วิธีวัด
1. ใส่ Scope หลัง EQ/compressor/limiter บนบัสที่ต้องการวัด ตรวจ input layout ที่หัวหน้าจอ
2. กด RESET แล้วเล่นเพลงจากต้นจนจบหนึ่งรอบ หลีกเลี่ยง seek และ loop เพราะค่ารวมจะนับข้อมูลที่เล่นซ้ำ ไม่มี transport reset อัตโนมัติ
3. เลือก Reference ดูส่วนต่าง LUFS และ true peak เทียบ reference
4. รอประมาณครึ่งวินาทีหลังจบเพื่อให้ worker จัดการคิวที่เหลือ
5. PAUSE หยุดรับข้อมูลวิเคราะห์ใหม่ แต่เสียงและ transport ยังเดิน คิวที่รับไว้ก่อน pause ยังวิเคราะห์ต่อ
6. หากขึ้น INCOMPLETE MEASUREMENT แปลว่าคิวข้อมูลเต็ม โดยเฉพาะ offline render เร็วมาก ให้ reset และเล่นใหม่แบบ real time ค่าที่เก็บไม่ครบใช้รับรองไม่ได้
7. Save project เก็บ profile/custom settings แต่ไม่เก็บประวัติ loudness หรือสถานะ pause เมื่อเปิดใหม่ต้องวัดใหม่

เสียงผ่านตรง ไม่ปรับ gain และไม่เพิ่ม audio latency ส่วนการแสดงผลมีความหน่วงจาก worker
มีชุดทดสอบ calibration sine, M/S/I windows, channel weights, LFE, silence gate และ inter-sample peaks เตรียมให้ CI แต่ยังไม่ได้รันที่นี่
ยังไม่ผ่าน official conformance vectors, การตรวจ VAD accuracy หรือ host/UI tests จึงไม่อ้าง BS.1770/EBU/Dolby certification

รองรับ sample rate หลัก 44.1/48/88.2/96/176.4/192 kHz ทั้งนี้ต้องทดสอบใน DAW จริง
