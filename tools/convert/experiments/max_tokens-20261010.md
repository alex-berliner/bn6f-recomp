# Max output tokens x Hyper's cheap models (2026-10-10)

16 functions, one first-round answer per (model, cap), farm prompt at commit dd222d2, streamed unless noted. Reproduce: see the docstring of max_tokens.py; functions in max_tokens-20261010/functions.txt.

Caveat: the prompt asked for `bool` but game.h lacked stdbool.h then (fixed in 14350e6); 215 compile errors came from that, so pass counts are low for every cell. The cap and runaway columns are unaffected.

| model | cap | calls | hit cap | no code | errors | passed | mean out tokens | mean s | passed per M out |
|---|---|---|---|---|---|---|---|---|---|
| deepseek-v4.1-flash | 24k | 16 | 10 | 10 | 0 | 2 | 20754 | 186 | 6.0 |
| deepseek-v4.1-flash | 48k | 16 | 4 | 4 | 0 | 2 | 31511 | 280 | 4.0 |
| deepseek-v4.1-flash | 72k | 16 | 3 | 3 | 0 | 3 | 34519 | 299 | 5.4 |
| deepseek-v4.1-flash | 96k | 16 | 0 | 0 | 0 | 2 | 33166 | 283 | 3.8 |
| deepseek-v4.1-flash | 120k | 16 | 0 | 0 | 0 | 2 | 30915 | 269 | 4.0 |
| glm-5.3-flash | 24k | 16 | 5 | 5 | 0 | 2 | 17128 | 244 | 7.3 |
| glm-5.3-flash | 48k | 16 | 2 | 1 | 0 | 3 | 26059 | 404 | 7.2 |
| glm-5.3-flash | 72k | 16 | 1 | 1 | 0 | 3 | 22755 | 348 | 8.2 |
| glm-5.3-flash | 96k | 16 | 0 | 0 | 0 | 2 | 29458 | 453 | 4.2 |
| glm-5.3-flash | 120k | 16 | 0 | 0 | 0 | 2 | 26984 | 420 | 4.6 |
| qwen3.8-flash | 24k | 16 | 0 | 0 | 0 | 4 | 12704 | 199 | 19.7 |
| qwen3.8-flash | 48k | 16 | 0 | 0 | 0 | 2 | 13116 | 203 | 9.5 |
| qwen3.8-flash | 72k | 16 | 0 | 0 | 0 | 3 | 15330 | 238 | 12.2 |
| qwen3.8-flash | 96k | 16 | 0 | 0 | 0 | 3 | 13935 | 213 | 13.5 |
| qwen3.8-flash | 120k | 16 | 0 | 0 | 0 | 2 | 13498 | 208 | 9.3 |

| model | cap | calls | hit cap | no code | errors | passed | mean out tokens | mean s | passed per M out |
|---|---|---|---|---|---|---|---|---|---|
| glm-5.3-flash (no stream) | 24k | 16 | 8 | 8 | 0 | 4 | 17437 | 208 | 14.3 |
| glm-5.3-flash (no stream) | 48k | 16 | 0 | 0 | 0 | 2 | 17199 | 305 | 7.3 |

Findings: qwen3.8-flash never ran away (max 33k tokens at any cap) and used about half the tokens of the others; glm-5.3-flash and deepseek-v4.1-flash spend the whole cap on reasoning in 30-60% of calls at 24k and stop running away only at 96k. Streaming made no difference (glm no-stream 24k: 8/16 runaways). Cap had no visible effect on pass counts (2-4 of 16 in every cell). Next: the farm runs on qwen3.8-flash.

## Re-test after the stdbool fix (14350e6; same answers, no model calls: max_tokens.py OUT --retest)

| model | cap | calls | hit cap | no code | errors | passed | mean out tokens | mean s | passed per M out |
|---|---|---|---|---|---|---|---|---|---|
| deepseek-v4.1-flash | 24k | 16 | 10 | 10 | 0 | 2 | 20754 | 186 | 6.0 |
| deepseek-v4.1-flash | 48k | 16 | 4 | 4 | 0 | 2 | 31511 | 280 | 4.0 |
| deepseek-v4.1-flash | 72k | 16 | 3 | 3 | 0 | 3 | 34519 | 299 | 5.4 |
| deepseek-v4.1-flash | 96k | 16 | 0 | 0 | 0 | 3 | 33166 | 283 | 5.7 |
| deepseek-v4.1-flash | 120k | 16 | 0 | 0 | 0 | 4 | 30915 | 269 | 8.1 |
| glm-5.3-flash | 24k | 16 | 5 | 5 | 0 | 3 | 17128 | 244 | 10.9 |
| glm-5.3-flash | 48k | 16 | 2 | 1 | 0 | 4 | 26059 | 404 | 9.6 |
| glm-5.3-flash | 72k | 16 | 1 | 1 | 0 | 3 | 22755 | 348 | 8.2 |
| glm-5.3-flash | 96k | 16 | 0 | 0 | 0 | 5 | 29458 | 453 | 10.6 |
| glm-5.3-flash | 120k | 16 | 0 | 0 | 0 | 4 | 26984 | 420 | 9.3 |
| qwen3.8-flash | 24k | 16 | 0 | 0 | 0 | 4 | 12704 | 199 | 19.7 |
| qwen3.8-flash | 48k | 16 | 0 | 0 | 0 | 3 | 13116 | 203 | 14.3 |
| qwen3.8-flash | 72k | 16 | 0 | 0 | 0 | 4 | 15330 | 238 | 16.3 |
| qwen3.8-flash | 96k | 16 | 0 | 0 | 0 | 4 | 13935 | 213 | 17.9 |
| qwen3.8-flash | 120k | 16 | 0 | 0 | 0 | 3 | 13498 | 208 | 13.9 |

| model | cap | calls | hit cap | no code | errors | passed | mean out tokens | mean s | passed per M out |
|---|---|---|---|---|---|---|---|---|---|
| glm-5.3-flash (no stream) | 24k | 16 | 8 | 8 | 0 | 4 | 17437 | 208 | 14.3 |
| glm-5.3-flash (no stream) | 48k | 16 | 0 | 0 | 0 | 3 | 17199 | 305 | 10.9 |

Passed out of 80 answers: glm-5.3-flash 19, qwen3.8-flash 18, deepseek-v4.1-flash 14; per million output tokens qwen
passes about twice as many as glm. Remaining failures (240 answers): compile 96, behaviour 41, review rules 20, register
check 17, style 8. Most compile errors are struct-offset names the assembly uses (oToolkit_JoypadPtr,
oGFXAnimState_Timer, ...) that cport/include/bn6f_syms.h lacks, since it only holds the constants lifted code uses:
6 of the 16 functions failed this way for every model.

## Re-test after bn6f_equates.h (8549708: every assembler constant available to hand-written code)

| model | cap | calls | hit cap | no code | errors | passed | mean out tokens | mean s | passed per M out |
|---|---|---|---|---|---|---|---|---|---|
| deepseek-v4.1-flash | 24k | 16 | 10 | 10 | 0 | 3 | 20754 | 186 | 9.0 |
| deepseek-v4.1-flash | 48k | 16 | 4 | 4 | 0 | 5 | 31511 | 280 | 9.9 |
| deepseek-v4.1-flash | 72k | 16 | 3 | 3 | 0 | 5 | 34519 | 299 | 9.1 |
| deepseek-v4.1-flash | 96k | 16 | 0 | 0 | 0 | 6 | 33166 | 283 | 11.3 |
| deepseek-v4.1-flash | 120k | 16 | 0 | 0 | 0 | 7 | 30915 | 269 | 14.2 |
| glm-5.3-flash | 24k | 16 | 5 | 5 | 0 | 8 | 17128 | 244 | 29.2 |
| glm-5.3-flash | 48k | 16 | 2 | 1 | 0 | 6 | 26059 | 404 | 14.4 |
| glm-5.3-flash | 72k | 16 | 1 | 1 | 0 | 5 | 22755 | 348 | 13.7 |
| glm-5.3-flash | 96k | 16 | 0 | 0 | 0 | 8 | 29458 | 453 | 17.0 |
| glm-5.3-flash | 120k | 16 | 0 | 0 | 0 | 5 | 26984 | 420 | 11.6 |
| qwen3.8-flash | 24k | 16 | 0 | 0 | 0 | 6 | 12704 | 199 | 29.5 |
| qwen3.8-flash | 48k | 16 | 0 | 0 | 0 | 5 | 13116 | 203 | 23.8 |
| qwen3.8-flash | 72k | 16 | 0 | 0 | 0 | 7 | 15330 | 238 | 28.5 |
| qwen3.8-flash | 96k | 16 | 0 | 0 | 0 | 6 | 13935 | 213 | 26.9 |
| qwen3.8-flash | 120k | 16 | 0 | 0 | 0 | 4 | 13498 | 208 | 18.5 |

| model | cap | calls | hit cap | no code | errors | passed | mean out tokens | mean s | passed per M out |
|---|---|---|---|---|---|---|---|---|---|
| glm-5.3-flash (no stream) | 24k | 16 | 8 | 8 | 0 | 5 | 17437 | 208 | 17.9 |
| glm-5.3-flash (no stream) | 48k | 16 | 0 | 0 | 0 | 7 | 17199 | 305 | 25.4 |

Passed out of 80 answers: glm-5.3-flash 32, qwen3.8-flash 28, deepseek-v4.1-flash 26 (all three: 58 -> 86 from this fix
alone). Per million output tokens: qwen about 25, glm about 16, deepseek about 11.
