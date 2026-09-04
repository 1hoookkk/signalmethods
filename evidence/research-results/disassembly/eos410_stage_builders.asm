==============================================================================
build_lpf_stages  @ 0x0C0AD4 .. 0x0C0CA4  (464 bytes)
==============================================================================
0C0AD4  4e 56 00 00               link.w a6, #$0
0C0AD8  48 e7 3e 20               movem.l d2-d6/a2, -(a7)
0C0ADC  24 6e 00 08               movea.l $8(a6), a2
0C0AE0  24 2e 00 10               move.l $10(a6), d2
0C0AE4  2a 2e 00 14               move.l $14(a6), d5
0C0AE8  26 3c ff cf ff cf         move.l #$ffcfffcf, d3
0C0AEE  7c 03                     moveq #$3, d6
0C0AF0  bc 85                     cmp.l d5, d6
0C0AF2  66 06                     bne.b $c0afa
0C0AF4  20 02                     move.l d2, d0
0C0AF6  e4 80                     asr.l #$2, d0
0C0AF8  94 80                     sub.l d0, d2
0C0AFA  e9 c2 26 44               bfextu d2{25:4}, d2
0C0AFE  41 fa ff 54               lea.l $c0a54(pc), a0
0C0B02  28 30 2c 00               move.l (a0, d2.l * 4), d4
0C0B06  48 78 00 40               pea.l $40.w
0C0B0A  2f 2e 00 0c               move.l $c(a6), -(a7)
0C0B0E  2f 0a                     move.l a2, -(a7)
0C0B10  4e b9 00 08 9e dc         jsr $89edc.l
0C0B16  7c 01                     moveq #$1, d6
0C0B18  bc 85                     cmp.l d5, d6
0C0B1A  6d 34                     blt.b $c0b50
0C0B1C  25 43 00 04               move.l d3, $4(a2)
0C0B20  25 43 00 14               move.l d3, $14(a2)
0C0B24  41 fa fe ee               lea.l $c0a14(pc), a0
0C0B28  20 30 2c 00               move.l (a0, d2.l * 4), d0
0C0B2C  e2 88                     lsr.l #$1, d0
0C0B2E  90 b9 00 f0 1e fc         sub.l $f01efc.l, d0
0C0B34  44 80                     neg.l d0
0C0B36  25 40 00 24               move.l d0, $24(a2)
0C0B3A  24 84                     move.l d4, (a2)
0C0B3C  25 43 00 10               move.l d3, $10(a2)
0C0B40  25 43 00 20               move.l d3, $20(a2)
0C0B44  41 fa ff 4e               lea.l $c0a94(pc), a0
0C0B48  20 30 2c 00               move.l (a0, d2.l * 4), d0
0C0B4C  e2 80                     asr.l #$1, d0
0C0B4E  60 6a                     bra.b $c0bba
0C0B50  7c 02                     moveq #$2, d6
0C0B52  bc 85                     cmp.l d5, d6
0C0B54  67 38                     beq.b $c0b8e
0C0B56  25 43 00 04               move.l d3, $4(a2)
0C0B5A  25 43 00 14               move.l d3, $14(a2)
0C0B5E  41 fa fe b4               lea.l $c0a14(pc), a0
0C0B62  22 39 00 f0 1e fc         move.l $f01efc.l, d1
0C0B68  92 b0 2c 00               sub.l (a0, d2.l * 4), d1
0C0B6C  20 30 2c 00               move.l (a0, d2.l * 4), d0
0C0B70  e2 88                     lsr.l #$1, d0
0C0B72  92 80                     sub.l d0, d1
0C0B74  25 41 00 24               move.l d1, $24(a2)
0C0B78  24 84                     move.l d4, (a2)
0C0B7A  25 44 00 10               move.l d4, $10(a2)
0C0B7E  25 44 00 20               move.l d4, $20(a2)
0C0B82  41 fa ff 10               lea.l $c0a94(pc), a0
0C0B86  20 30 2c 00               move.l (a0, d2.l * 4), d0
0C0B8A  d0 80                     add.l d0, d0
0C0B8C  60 2c                     bra.b $c0bba
0C0B8E  25 43 00 04               move.l d3, $4(a2)
0C0B92  25 43 00 14               move.l d3, $14(a2)
0C0B96  41 fa fe 7c               lea.l $c0a14(pc), a0
0C0B9A  20 39 00 f0 1e fc         move.l $f01efc.l, d0
0C0BA0  90 b0 2c 00               sub.l (a0, d2.l * 4), d0
0C0BA4  25 40 00 24               move.l d0, $24(a2)
0C0BA8  24 84                     move.l d4, (a2)
0C0BAA  25 44 00 10               move.l d4, $10(a2)
0C0BAE  25 43 00 20               move.l d3, $20(a2)
0C0BB2  41 fa fe e0               lea.l $c0a94(pc), a0
0C0BB6  20 30 2c 00               move.l (a0, d2.l * 4), d0
0C0BBA  4c ee 04 7c ff e8         movem.l -$18(a6), d2-d6/a2
0C0BC0  4e 5e                     unlk a6
0C0BC2  4e 75                     rts 
   --- rts; remaining 224 bytes to next function ---
   TAIL DATA 0x0C0BC4..0x0C0CA4:
     0C0BC4  4e 56 00 00 2f 02 22 39 00 f0 33 92 30 7c 00 ff
     0C0BD4  91 ee 00 08 20 08 53 48 6f 1a 32 7c 03 ff 4c 39
     0C0BE4  18 00 00 f0 33 8e 6a 02 d2 89 74 0a e4 a1 20 08
     0C0BF4  53 48 6e ea 20 01 24 2e ff fc 4e 5e 4e 75 46 72
     0C0C04  65 71 75 65 6e 63 79 00 25 64 48 7a 00 51 00 25
     0C0C14  64 00 4e 56 00 00 2f 02 20 6e 00 10 24 2e 00 14
     0C0C24  10 2e 00 0b 32 2e 00 0e 4a 00 67 08 0c 00 00 01
     0C0C34  67 42 60 5c 10 fa ff c8 10 fa ff c5 10 fa ff c2
     0C0C44  10 fa ff bf 10 fa ff bc 10 fa ff b9 10 fa ff b6
     0C0C54  10 fa ff b3 10 fa ff b0 10 ba ff ad 48 78 03 ea
     0C0C64  48 78 4e 20 32 41 2f 09 4e ba fc dc 2f 00 48 7a
     0C0C74  ff 98 60 12 10 ba ff 97 11 7a ff 94 00 01 32 41
     0C0C84  2f 09 48 7a ff 8b 2f 02 4e b9 00 02 83 de 60 08
     0C0C94  2f 02 2f 08 4e ba fc 30 24 2e ff fc 4e 5e 4e 75
==============================================================================
build_hpf_stages  @ 0x0C0CA4 .. 0x0C0DCA  (294 bytes)
==============================================================================
0C0CA4  4e 56 00 00               link.w a6, #$0
0C0CA8  48 e7 38 00               movem.l d2-d4, -(a7)
0C0CAC  20 6e 00 08               movea.l $8(a6), a0
0C0CB0  26 2e 00 10               move.l $10(a6), d3
0C0CB4  20 2e 00 14               move.l $14(a6), d0
0C0CB8  e4 83                     asr.l #$2, d3
0C0CBA  20 bc ff cf ff cf         move.l #$ffcfffcf, (a0)
0C0CC0  21 7c 80 04 80 04 00 04   move.l #$80048004, $4(a0)
0C0CC8  22 03                     move.l d3, d1
0C0CCA  e1 81                     asl.l #$8, d1
0C0CCC  04 81 80 04 ff cf         subi.l #$8004ffcf, d1
0C0CD2  44 81                     neg.l d1
0C0CD4  24 03                     move.l d3, d2
0C0CD6  78 18                     moveq #$18, d4
0C0CD8  e9 a2                     asl.l d4, d2
0C0CDA  92 82                     sub.l d2, d1
0C0CDC  21 41 00 10               move.l d1, $10(a0)
0C0CE0  78 01                     moveq #$1, d4
0C0CE2  b8 80                     cmp.l d0, d4
0C0CE4  6d 12                     blt.b $c0cf8
0C0CE6  21 7c ff cf ff cf 00 14   move.l #$ffcfffcf, $14(a0)
0C0CEE  21 7c ff cf ff cf 00 20   move.l #$ffcfffcf, $20(a0)
0C0CF6  60 20                     bra.b $c0d18
0C0CF8  21 7c 80 04 80 04 00 14   move.l #$80048004, $14(a0)
0C0D00  20 03                     move.l d3, d0
0C0D02  e1 80                     asl.l #$8, d0
0C0D04  04 80 80 04 ff cf         subi.l #$8004ffcf, d0
0C0D0A  44 80                     neg.l d0
0C0D0C  22 03                     move.l d3, d1
0C0D0E  78 18                     moveq #$18, d4
0C0D10  e9 a1                     asl.l d4, d1
0C0D12  90 81                     sub.l d1, d0
0C0D14  21 40 00 20               move.l d0, $20(a0)
0C0D18  21 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a0)
0C0D20  48 78 ff c0               pea.l $ffc0.w
0C0D24  2f 2e 00 0c               move.l $c(a6), -(a7)
0C0D28  2f 08                     move.l a0, -(a7)
0C0D2A  4e b9 00 08 9e dc         jsr $89edc.l
0C0D30  42 80                     clr.l d0
0C0D32  4c ee 00 1c ff f4         movem.l -$c(a6), d2-d4
0C0D38  4e 5e                     unlk a6
0C0D3A  4e 75                     rts 
   --- rts; remaining 142 bytes to next function ---
   TAIL DATA 0x0C0D3C..0x0C0DCA:
     0C0D3C  4e 56 00 00 2f 02 20 6e 00 10 24 2e 00 14 10 2e
     0C0D4C  00 0b 32 2e 00 0e 4a 00 67 08 0c 00 00 01 67 42
     0C0D5C  60 5c 10 fa fe a2 10 fa fe 9f 10 fa fe 9c 10 fa
     0C0D6C  fe 99 10 fa fe 96 10 fa fe 93 10 fa fe 90 10 fa
     0C0D7C  fe 8d 10 fa fe 8a 10 ba fe 87 48 78 03 eb 48 78
     0C0D8C  46 50 32 41 2f 09 4e ba fb b6 2f 00 48 7a fe 72
     0C0D9C  60 12 10 ba fe 71 11 7a fe 6e 00 01 32 41 2f 09
     0C0DAC  48 7a fe 65 2f 02 4e b9 00 02 83 de 60 08 2f 02
     0C0DBC  2f 08 4e ba fb 0a 24 2e ff fc 4e 5e 4e 75
==============================================================================
build_swept_eq_stages  @ 0x0C0DCA .. 0x0C0FAE  (484 bytes)
==============================================================================
0C0DCA  4e 56 00 00               link.w a6, #$0
0C0DCE  2f 0a                     move.l a2, -(a7)
0C0DD0  2f 02                     move.l d2, -(a7)
0C0DD2  22 6e 00 08               movea.l $8(a6), a1
0C0DD6  20 2e 00 14               move.l $14(a6), d0
0C0DDA  24 6e 00 10               movea.l $10(a6), a2
0C0DDE  41 ea ff c0               lea.l -$40(a2), a0
0C0DE2  24 08                     move.l a0, d2
0C0DE4  e2 82                     asr.l #$1, d2
0C0DE6  20 42                     movea.l d2, a0
0C0DE8  22 bc ff cf ff cf         move.l #$ffcfffcf, (a1)
0C0DEE  23 7c ff cf ff cf 00 04   move.l #$ffcfffcf, $4(a1)
0C0DF6  23 7c ff cf ff cf 00 10   move.l #$ffcfffcf, $10(a1)
0C0DFE  23 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a1)
0C0E06  74 01                     moveq #$1, d2
0C0E08  b4 80                     cmp.l d0, d2
0C0E0A  6d 22                     blt.b $c0e2e
0C0E0C  72 77                     moveq #$77, d1
0C0E0E  d2 88                     add.l a0, d1
0C0E10  74 18                     moveq #$18, d2
0C0E12  e5 a1                     asl.l d2, d1
0C0E14  20 08                     move.l a0, d0
0C0E16  06 80 00 00 00 df         addi.l #$df, d0
0C0E1C  e1 80                     asl.l #$8, d0
0C0E1E  00 80 00 07 00 d7         ori.l #$700d7, d0
0C0E24  82 80                     or.l d0, d1
0C0E26  23 41 00 14               move.l d1, $14(a1)
0C0E2A  72 77                     moveq #$77, d1
0C0E2C  60 56                     bra.b $c0e84
0C0E2E  74 02                     moveq #$2, d2
0C0E30  b4 80                     cmp.l d0, d2
0C0E32  66 2a                     bne.b $c0e5e
0C0E34  22 08                     move.l a0, d1
0C0E36  06 81 00 00 00 91         addi.l #$91, d1
0C0E3C  74 18                     moveq #$18, d2
0C0E3E  e5 a1                     asl.l d2, d1
0C0E40  20 08                     move.l a0, d0
0C0E42  06 80 00 00 00 df         addi.l #$df, d0
0C0E48  e1 80                     asl.l #$8, d0
0C0E4A  00 80 00 07 00 d7         ori.l #$700d7, d0
0C0E50  82 80                     or.l d0, d1
0C0E52  23 41 00 14               move.l d1, $14(a1)
0C0E56  22 3c 00 00 00 91         move.l #$91, d1
0C0E5C  60 26                     bra.b $c0e84
0C0E5E  22 08                     move.l a0, d1
0C0E60  06 81 00 00 00 a4         addi.l #$a4, d1
0C0E66  74 18                     moveq #$18, d2
0C0E68  e5 a1                     asl.l d2, d1
0C0E6A  20 08                     move.l a0, d0
0C0E6C  06 80 00 00 00 df         addi.l #$df, d0
0C0E72  e1 80                     asl.l #$8, d0
0C0E74  00 80 00 07 00 d7         ori.l #$700d7, d0
0C0E7A  82 80                     or.l d0, d1
0C0E7C  23 41 00 14               move.l d1, $14(a1)
0C0E80  72 52                     moveq #$52, d1
0C0E82  d2 41                     add.w d1, d1
0C0E84  92 88                     sub.l a0, d1
0C0E86  e5 a1                     asl.l d2, d1
0C0E88  20 3c 00 00 00 df         move.l #$df, d0
0C0E8E  90 88                     sub.l a0, d0
0C0E90  e1 80                     asl.l #$8, d0
0C0E92  00 80 00 07 00 d7         ori.l #$700d7, d0
0C0E98  82 80                     or.l d0, d1
0C0E9A  23 41 00 20               move.l d1, $20(a1)
0C0E9E  48 78 ff c0               pea.l $ffc0.w
0C0EA2  2f 2e 00 0c               move.l $c(a6), -(a7)
0C0EA6  2f 09                     move.l a1, -(a7)
0C0EA8  4e b9 00 08 9e dc         jsr $89edc.l
0C0EAE  70 fd                     moveq #$fd, d0
0C0EB0  24 2e ff f8               move.l -$8(a6), d2
0C0EB4  24 6e ff fc               movea.l -$4(a6), a2
0C0EB8  4e 5e                     unlk a6
0C0EBA  4e 75                     rts 
   --- rts; remaining 242 bytes to next function ---
   TAIL DATA 0x0C0EBC..0x0C0FAE:
     0C0EBC  47 61 69 6e 00 2b 00 2d 00 25 73 25 64 2e 25 31
     0C0ECC  64 64 42 00 4e 56 00 00 48 e7 38 00 20 6e 00 10
     0C0EDC  26 2e 00 14 10 2e 00 0b 32 2e 00 0e 4a 00 67 0a
     0C0EEC  0c 00 00 01 67 4c 60 00 00 a8 10 fa fd 0a 10 fa
     0C0EFC  fd 07 10 fa fd 04 10 fa fd 01 10 fa fc fe 10 fa
     0C0F0C  fc fb 10 fa fc f8 10 fa fc f5 10 fa fc f2 10 ba
     0C0F1C  fc ef 48 78 03 ee 48 78 27 10 32 41 2f 09 4e ba
     0C0F2C  fa 1e 2f 00 48 7a fc da 2f 03 4e b9 00 02 83 de
     0C0F3C  60 66 10 fa ff 7c 10 fa ff 79 10 fa ff 76 10 fa
     0C0F4C  ff 73 10 ba ff 70 c3 fc 00 78 6a 04 78 1f d2 84
     0C0F5C  ea 81 06 81 ff ff ff 10 24 01 78 0a 4c 44 28 00
     0C0F6C  4a 80 6c 02 44 80 2f 00 20 02 6c 02 44 80 2f 00
     0C0F7C  20 3c 00 0d 0e c3 4a 81 6d 06 20 3c 00 0d 0e c1
     0C0F8C  2f 00 48 7a ff 35 2f 03 4e b9 00 02 83 de 60 08
     0C0F9C  2f 03 2f 08 4e ba f9 28 4c ee 00 1c ff f4 4e 5e
     0C0FAC  4e 75
==============================================================================
build_bpf_stages  @ 0x0C0FAE .. 0x0C1176  (456 bytes)
==============================================================================
0C0FAE  4e 56 00 00               link.w a6, #$0
0C0FB2  2f 03                     move.l d3, -(a7)
0C0FB4  2f 02                     move.l d2, -(a7)
0C0FB6  20 6e 00 08               movea.l $8(a6), a0
0C0FBA  22 6e 00 0c               movea.l $c(a6), a1
0C0FBE  24 2e 00 10               move.l $10(a6), d2
0C0FC2  20 2e 00 14               move.l $14(a6), d0
0C0FC6  76 01                     moveq #$1, d3
0C0FC8  b6 80                     cmp.l d0, d3
0C0FCA  6d 58                     blt.b $c1024
0C0FCC  e2 82                     asr.l #$1, d2
0C0FCE  20 bc ff 68 ff cf         move.l #$ff68ffcf, (a0)
0C0FD4  21 7c ff 68 ff cf 00 04   move.l #$ff68ffcf, $4(a0)
0C0FDC  21 7c ff cf ff cf 00 10   move.l #$ffcfffcf, $10(a0)
0C0FE4  21 7c ff 04 ff 90 00 14   move.l #$ff04ff90, $14(a0)
0C0FEC  72 44                     moveq #$44, d1
0C0FEE  d2 41                     add.w d1, d1
0C0FF0  92 82                     sub.l d2, d1
0C0FF2  76 18                     moveq #$18, d3
0C0FF4  e7 a1                     asl.l d3, d1
0C0FF6  70 7c                     moveq #$7c, d0
0C0FF8  d0 40                     add.w d0, d0
0C0FFA  90 82                     sub.l d2, d0
0C0FFC  e1 80                     asl.l #$8, d0
0C0FFE  00 80 00 08 00 c8         ori.l #$800c8, d0
0C1004  82 80                     or.l d0, d1
0C1006  21 41 00 20               move.l d1, $20(a0)
0C100A  21 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a0)
0C1012  48 78 ff c0               pea.l $ffc0.w
0C1016  2f 09                     move.l a1, -(a7)
0C1018  2f 08                     move.l a0, -(a7)
0C101A  4e b9 00 08 9e dc         jsr $89edc.l
0C1020  e4 82                     asr.l #$2, d2
0C1022  60 58                     bra.b $c107c
0C1024  76 02                     moveq #$2, d3
0C1026  b6 80                     cmp.l d0, d3
0C1028  66 58                     bne.b $c1082
0C102A  e2 82                     asr.l #$1, d2
0C102C  72 44                     moveq #$44, d1
0C102E  d2 41                     add.w d1, d1
0C1030  92 82                     sub.l d2, d1
0C1032  76 18                     moveq #$18, d3
0C1034  e7 a1                     asl.l d3, d1
0C1036  70 7c                     moveq #$7c, d0
0C1038  d0 40                     add.w d0, d0
0C103A  90 82                     sub.l d2, d0
0C103C  e1 80                     asl.l #$8, d0
0C103E  00 80 00 08 00 c8         ori.l #$800c8, d0
0C1044  82 80                     or.l d0, d1
0C1046  20 81                     move.l d1, (a0)
0C1048  21 7c ff 04 ff 90 00 04   move.l #$ff04ff90, $4(a0)
0C1050  21 7c ff cf ff cf 00 10   move.l #$ffcfffcf, $10(a0)
0C1058  21 7c ff 04 ff 90 00 14   move.l #$ff04ff90, $14(a0)
0C1060  21 41 00 20               move.l d1, $20(a0)
0C1064  21 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a0)
0C106C  48 78 ff c0               pea.l $ffc0.w
0C1070  2f 09                     move.l a1, -(a7)
0C1072  2f 08                     move.l a0, -(a7)
0C1074  4e b9 00 08 9e dc         jsr $89edc.l
0C107A  e2 82                     asr.l #$1, d2
0C107C  20 02                     move.l d2, d0
0C107E  44 80                     neg.l d0
0C1080  60 5a                     bra.b $c10dc
0C1082  76 03                     moveq #$3, d3
0C1084  b6 80                     cmp.l d0, d3
0C1086  66 52                     bne.b $c10da
0C1088  72 44                     moveq #$44, d1
0C108A  d2 41                     add.w d1, d1
0C108C  92 82                     sub.l d2, d1
0C108E  76 18                     moveq #$18, d3
0C1090  e7 a1                     asl.l d3, d1
0C1092  70 68                     moveq #$68, d0
0C1094  d0 40                     add.w d0, d0
0C1096  90 82                     sub.l d2, d0
0C1098  e1 80                     asl.l #$8, d0
0C109A  00 80 00 08 00 c8         ori.l #$800c8, d0
0C10A0  82 80                     or.l d0, d1
0C10A2  20 81                     move.l d1, (a0)
0C10A4  21 7c ff cf ff cf 00 04   move.l #$ffcfffcf, $4(a0)
0C10AC  21 7c ff cf ff cf 00 10   move.l #$ffcfffcf, $10(a0)
0C10B4  21 7c ff d0 ff 20 00 14   move.l #$ffd0ff20, $14(a0)
0C10BC  21 7c ff cf ff cf 00 20   move.l #$ffcfffcf, $20(a0)
0C10C4  21 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a0)
0C10CC  48 78 ff c0               pea.l $ffc0.w
0C10D0  2f 09                     move.l a1, -(a7)
0C10D2  2f 08                     move.l a0, -(a7)
0C10D4  4e b9 00 08 9e dc         jsr $89edc.l
0C10DA  42 80                     clr.l d0
0C10DC  24 2e ff f8               move.l -$8(a6), d2
0C10E0  26 2e ff fc               move.l -$4(a6), d3
0C10E4  4e 5e                     unlk a6
0C10E6  4e 75                     rts 
   --- rts; remaining 142 bytes to next function ---
   TAIL DATA 0x0C10E8..0x0C1176:
     0C10E8  4e 56 00 00 2f 02 20 6e 00 10 24 2e 00 14 10 2e
     0C10F8  00 0b 32 2e 00 0e 4a 00 67 08 0c 00 00 01 67 42
     0C1108  60 5c 10 fa fa f6 10 fa fa f3 10 fa fa f0 10 fa
     0C1118  fa ed 10 fa fa ea 10 fa fa e7 10 fa fa e4 10 fa
     0C1128  fa e1 10 fa fa de 10 ba fa db 48 78 03 ed 48 78
     0C1138  27 10 32 41 2f 09 4e ba f8 0a 2f 00 48 7a fa c6
     0C1148  60 12 10 ba fa c5 11 7a fa c2 00 01 32 41 2f 09
     0C1158  48 7a fa b9 2f 02 4e b9 00 02 83 de 60 08 2f 02
     0C1168  2f 08 4e ba f7 5e 24 2e ff fc 4e 5e 4e 75
==============================================================================
build_phaser_stages  @ 0x0C1176 .. 0x0C121E  (168 bytes)
==============================================================================
0C1176  4e 56 00 00               link.w a6, #$0
0C117A  48 e7 38 00               movem.l d2-d4, -(a7)
0C117E  20 6e 00 08               movea.l $8(a6), a0
0C1182  26 2e 00 10               move.l $10(a6), d3
0C1186  20 2e 00 14               move.l $14(a6), d0
0C118A  e2 83                     asr.l #$1, d3
0C118C  24 03                     move.l d3, d2
0C118E  e1 82                     asl.l #$8, d2
0C1190  22 03                     move.l d3, d1
0C1192  e4 81                     asr.l #$2, d1
0C1194  84 81                     or.l d1, d2
0C1196  22 02                     move.l d2, d1
0C1198  48 41                     swap d1
0C119A  42 41                     clr.w d1
0C119C  84 81                     or.l d1, d2
0C119E  20 bc ff cf ff cf         move.l #$ffcfffcf, (a0)
0C11A4  78 01                     moveq #$1, d4
0C11A6  b8 80                     cmp.l d0, d4
0C11A8  6d 24                     blt.b $c11ce
0C11AA  21 7c 30 90 30 d0 00 04   move.l #$309030d0, $4(a0)
0C11B2  20 3c c0 90 c0 d0         move.l #$c090c0d0, d0
0C11B8  90 82                     sub.l d2, d0
0C11BA  21 40 00 10               move.l d0, $10(a0)
0C11BE  21 7c 30 50 30 90 00 14   move.l #$30503090, $14(a0)
0C11C6  20 3c c0 50 c0 90         move.l #$c050c090, d0
0C11CC  60 22                     bra.b $c11f0
0C11CE  21 7c 30 98 30 f2 00 04   move.l #$309830f2, $4(a0)
0C11D6  20 3c c0 98 c0 f2         move.l #$c098c0f2, d0
0C11DC  90 82                     sub.l d2, d0
0C11DE  21 40 00 10               move.l d0, $10(a0)
0C11E2  21 7c 30 30 30 b0 00 14   move.l #$303030b0, $14(a0)
0C11EA  20 3c c0 30 c0 b0         move.l #$c030c0b0, d0
0C11F0  90 82                     sub.l d2, d0
0C11F2  21 40 00 20               move.l d0, $20(a0)
0C11F6  21 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a0)
0C11FE  48 78 ff c0               pea.l $ffc0.w
0C1202  2f 2e 00 0c               move.l $c(a6), -(a7)
0C1206  2f 08                     move.l a0, -(a7)
0C1208  4e b9 00 08 9e dc         jsr $89edc.l
0C120E  20 03                     move.l d3, d0
0C1210  e4 80                     asr.l #$2, d0
0C1212  44 80                     neg.l d0
0C1214  4c ee 00 1c ff f4         movem.l -$c(a6), d2-d4
0C121A  4e 5e                     unlk a6
0C121C  4e 75                     rts 
   --- rts; remaining 0 bytes to next function ---
==============================================================================
build_bat_phaser_stages  @ 0x0C121E .. 0x0C1298  (122 bytes)
==============================================================================
0C121E  4e 56 00 00               link.w a6, #$0
0C1222  48 e7 38 00               movem.l d2-d4, -(a7)
0C1226  20 6e 00 08               movea.l $8(a6), a0
0C122A  26 2e 00 0c               move.l $c(a6), d3
0C122E  22 2e 00 10               move.l $10(a6), d1
0C1232  e2 81                     asr.l #$1, d1
0C1234  20 bc ff cf ff cf         move.l #$ffcfffcf, (a0)
0C123A  21 7c 00 10 00 c0 00 04   move.l #$1000c0, $4(a0)
0C1242  74 70                     moveq #$70, d2
0C1244  94 81                     sub.l d1, d2
0C1246  78 18                     moveq #$18, d4
0C1248  e9 a2                     asl.l d4, d2
0C124A  04 81 00 00 00 b0         subi.l #$b0, d1
0C1250  44 81                     neg.l d1
0C1252  e1 81                     asl.l #$8, d1
0C1254  20 01                     move.l d1, d0
0C1256  00 80 00 14 00 c4         ori.l #$1400c4, d0
0C125C  80 82                     or.l d2, d0
0C125E  21 40 00 10               move.l d0, $10(a0)
0C1262  21 7c 00 30 00 e0 00 14   move.l #$3000e0, $14(a0)
0C126A  00 81 00 2c 00 dc         ori.l #$2c00dc, d1
0C1270  84 81                     or.l d1, d2
0C1272  21 42 00 20               move.l d2, $20(a0)
0C1276  21 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a0)
0C127E  48 78 ff c0               pea.l $ffc0.w
0C1282  2f 03                     move.l d3, -(a7)
0C1284  2f 08                     move.l a0, -(a7)
0C1286  4e b9 00 08 9e dc         jsr $89edc.l
0C128C  42 80                     clr.l d0
0C128E  4c ee 00 1c ff f4         movem.l -$c(a6), d2-d4
0C1294  4e 5e                     unlk a6
0C1296  4e 75                     rts 
   --- rts; remaining 0 bytes to next function ---
==============================================================================
build_flanger_stages  @ 0x0C1298 .. 0x0C1416  (382 bytes)
==============================================================================
0C1298  4e 56 00 00               link.w a6, #$0
0C129C  2f 03                     move.l d3, -(a7)
0C129E  2f 02                     move.l d2, -(a7)
0C12A0  22 6e 00 08               movea.l $8(a6), a1
0C12A4  22 2e 00 10               move.l $10(a6), d1
0C12A8  24 2e 00 14               move.l $14(a6), d2
0C12AC  26 01                     move.l d1, d3
0C12AE  e8 83                     asr.l #$4, d3
0C12B0  20 03                     move.l d3, d0
0C12B2  48 40                     swap d0
0C12B4  42 40                     clr.w d0
0C12B6  86 80                     or.l d0, d3
0C12B8  e4 81                     asr.l #$2, d1
0C12BA  e1 89                     lsl.l #$8, d1
0C12BC  20 01                     move.l d1, d0
0C12BE  48 40                     swap d0
0C12C0  42 40                     clr.w d0
0C12C2  82 80                     or.l d0, d1
0C12C4  22 bc ff cf fa c0         move.l #$ffcffac0, (a1)
0C12CA  0c 82 00 00 03 e8         cmpi.l #$3e8, d2
0C12D0  6e 2c                     bgt.b $c12fe
0C12D2  23 7c 24 0f 54 6f 00 04   move.l #$240f546f, $4(a1)
0C12DA  20 43                     movea.l d3, a0
0C12DC  d1 fc 8f 13 c0 74         adda.l #$8f13c074, a0
0C12E2  91 c1                     suba.l d1, a0
0C12E4  23 48 00 10               move.l a0, $10(a1)
0C12E8  23 7c 14 03 44 4f 00 14   move.l #$1403444f, $14(a1)
0C12F0  20 03                     move.l d3, d0
0C12F2  06 80 78 03 a8 52         addi.l #$7803a852, d0
0C12F8  90 81                     sub.l d1, d0
0C12FA  23 40 00 20               move.l d0, $20(a1)
0C12FE  23 79 00 f0 1e fc 00 24   move.l $f01efc.l, $24(a1)
0C1306  48 78 ff c0               pea.l $ffc0.w
0C130A  2f 2e 00 0c               move.l $c(a6), -(a7)
0C130E  2f 09                     move.l a1, -(a7)
0C1310  4e b9 00 08 9e dc         jsr $89edc.l
0C1316  42 80                     clr.l d0
0C1318  24 2e ff f8               move.l -$8(a6), d2
0C131C  26 2e ff fc               move.l -$4(a6), d3
0C1320  4e 5e                     unlk a6
0C1322  4e 75                     rts 
   --- rts; remaining 242 bytes to next function ---
   TAIL DATA 0x0C1324..0x0C1416:
     0C1324  4e 56 00 00 20 6e 00 08 22 2e 00 0c 21 7c 00 a0
     0C1334  00 c0 00 24 20 bc d0 b0 d0 c0 21 7c 00 80 00 a0
     0C1344  00 04 20 3c a0 70 d0 a0 21 40 00 10 21 7c 00 60
     0C1354  00 80 00 14 20 3c b0 40 d0 70 21 40 00 20 48 78
     0C1364  ff 80 2f 01 2f 08 4e b9 00 08 9e dc 42 80 10 39
     0C1374  00 f0 1e fa 44 80 4e 5e 4e 75 52 65 73 6f 6e 61
     0C1384  6e 63 65 00 4e 56 00 00 20 6e 00 10 22 2e 00 14
     0C1394  10 2e 00 0b 32 6e 00 0e 67 08 0c 00 00 01 67 2c
     0C13A4  60 64 10 fa f8 5a 10 fa f8 57 10 fa f8 54 10 fa
     0C13B4  f8 51 10 fa f8 4e 10 fa f8 4b 10 fa f8 48 10 fa
     0C13C4  f8 45 10 fa f8 42 10 ba f8 3f 60 28 10 fa ff ac
     0C13D4  10 fa ff a9 10 fa ff a6 10 fa ff a3 10 fa ff a0
     0C13E4  10 fa ff 9d 10 fa ff 9a 10 fa ff 97 10 fa ff 94
     0C13F4  10 ba ff 91 32 49 2f 09 48 7a f8 15 2f 01 4e b9
     0C1404  00 02 83 de 60 08 2f 01 2f 08 4e ba f4 ba 4e 5e
     0C1414  4e 75
==============================================================================
build_vocal_aah_stages  @ 0x0C1416 .. 0x0C148A  (116 bytes)
==============================================================================
0C1416  4e 56 00 00               link.w a6, #$0
0C141A  2f 02                     move.l d2, -(a7)
0C141C  22 6e 00 08               movea.l $8(a6), a1
0C1420  24 2e 00 0c               move.l $c(a6), d2
0C1424  20 2e 00 10               move.l $10(a6), d0
0C1428  e4 80                     asr.l #$2, d0
0C142A  22 00                     move.l d0, d1
0C142C  48 41                     swap d1
0C142E  42 41                     clr.w d1
0C1430  82 80                     or.l d0, d1
0C1432  20 41                     movea.l d1, a0
0C1434  d1 fc 70 95 70 b1         adda.l #$709570b1, a0
0C143A  22 88                     move.l a0, (a1)
0C143C  23 7c ff c0 ff c0 00 04   move.l #$ffc0ffc0, $4(a1)
0C1444  20 41                     movea.l d1, a0
0C1446  d1 fc 70 6f 70 9d         adda.l #$706f709d, a0
0C144C  23 48 00 10               move.l a0, $10(a1)
0C1450  23 7c ff cf ff cf 00 14   move.l #$ffcfffcf, $14(a1)
0C1458  06 81 70 54 70 35         addi.l #$70547035, d1
0C145E  23 41 00 20               move.l d1, $20(a1)
0C1462  23 7c ff cf ff cf 00 24   move.l #$ffcfffcf, $24(a1)
0C146A  42 a7                     clr.l -(a7)
0C146C  2f 02                     move.l d2, -(a7)
0C146E  2f 09                     move.l a1, -(a7)
0C1470  4e b9 00 08 9e dc         jsr $89edc.l
0C1476  42 80                     clr.l d0
0C1478  10 39 00 f0 1e fa         move.b $f01efa.l, d0
0C147E  57 80                     subq.l #$3, d0
0C1480  44 80                     neg.l d0
0C1482  24 2e ff fc               move.l -$4(a6), d2
0C1486  4e 5e                     unlk a6
0C1488  4e 75                     rts 
   --- rts; remaining 0 bytes to next function ---
==============================================================================
build_vocal_ooh_stages  @ 0x0C148A .. 0x0C1520  (150 bytes)
==============================================================================
0C148A  4e 56 00 00               link.w a6, #$0
0C148E  2f 02                     move.l d2, -(a7)
0C1490  22 6e 00 08               movea.l $8(a6), a1
0C1494  24 2e 00 0c               move.l $c(a6), d2
0C1498  20 2e 00 10               move.l $10(a6), d0
0C149C  e4 80                     asr.l #$2, d0
0C149E  22 00                     move.l d0, d1
0C14A0  48 41                     swap d1
0C14A2  42 41                     clr.w d1
0C14A4  82 80                     or.l d0, d1
0C14A6  20 41                     movea.l d1, a0
0C14A8  d1 fc 90 a3 90 99         adda.l #$90a39099, a0
0C14AE  22 88                     move.l a0, (a1)
0C14B0  23 7c ff c0 ff c0 00 04   move.l #$ffc0ffc0, $4(a1)
0C14B8  20 41                     movea.l d1, a0
0C14BA  d1 fc 80 60 90 8a         adda.l #$8060908a, a0
0C14C0  23 48 00 10               move.l a0, $10(a1)
0C14C4  23 7c ff cf ff cf 00 14   move.l #$ffcfffcf, $14(a1)
0C14CC  06 81 50 34 70 60         addi.l #$50347060, d1
0C14D2  23 41 00 20               move.l d1, $20(a1)
0C14D6  23 7c ff cf ff cf 00 24   move.l #$ffcfffcf, $24(a1)
0C14DE  42 a7                     clr.l -(a7)
0C14E0  2f 02                     move.l d2, -(a7)
0C14E2  2f 09                     move.l a1, -(a7)
0C14E4  4e b9 00 08 9e dc         jsr $89edc.l
0C14EA  42 80                     clr.l d0
0C14EC  10 39 00 f0 1e fa         move.b $f01efa.l, d0
0C14F2  57 80                     subq.l #$3, d0
0C14F4  44 80                     neg.l d0
0C14F6  24 2e ff fc               move.l -$4(a6), d2
0C14FA  4e 5e                     unlk a6
0C14FC  4e 75                     rts 
   --- rts; remaining 34 bytes to next function ---
   TAIL DATA 0x0C14FE..0x0C1520:
     0C14FE  4d 6f 72 70 68 00 42 6f 64 79 20 53 69 7a 65 00
     0C150E  4e 56 00 00 20 6e 00 10 22 2e 00 14 10 2e 00 0b
     0C151E  32 6e
