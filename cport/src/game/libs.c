/*
 * libs.c: readable functions of bn6f libs.s, in address order.
 *
 * Laid out by tools/layout.py: a function converted on its own (the model farm) lives in
 * the file of the bn6f source that defines it, a big source in parts (tools/layout.txt).
 * Each function's part starts at its `── NAME ──` line and holds what it needs.
 */
#include "game.h"
#include "runtime.h"

/* ── sub_81440D8 ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r1, r2, r3; } call_SWI_CpuSet_result;
/* register form: generated wrapper: SWI_CpuSet has no typed C interface yet */
static inline call_SWI_CpuSet_result call_SWI_CpuSet(u32 r0, u32 r1, u32 r2) {
    R0 = r0;
    R1 = r1;
    R2 = r2;
    GUEST_CALL(SWI_CpuSet);
    return (call_SWI_CpuSet_result){ R1, R2, R3 };
}

/* sub_81440D8 - shut down the link-communication hardware (run before the
 * game hands control over to different hardware/mode): back up and mask the
 * interrupt state, stop SIO and Timer 3, then BIOS-zero the timer-enable
 * block and the SIO buffer at 0x200F44C.
 *
 * register form: the CpuSet source operand is a scratch word on the guest
 * stack whose address must be handed to the BIOS, so SP is read and adjusted
 * here (SP -= 8 ... SP += 8); a plain C buffer would not live at the guest
 * stack address the call needs. */
static call_SWI_CpuSet_result reset_comm_hardware(void) {
    u32 ime = LD16(A_InterruptMasterEnableRegister);
    u32 ie;
    call_SWI_CpuSet_result last;

    /* Save IME, disable interrupts, trim IE to 0xFF3F (clears the timer IRQ
     * sources), then restore IME. */
    ST16(A_word_2006DFC, ime);
    ST16(A_InterruptMasterEnableRegister, 0);
    ie = LD16(A_InterruptEnableRegister);
    ST16(A_InterruptEnableRegister, ie & 0xFF3F);
    ST16(A_InterruptMasterEnableRegister, ime);

    /* SIOCNT = 0x2000 (0x80 << 6) stops the serial engine, Timer 3 is
     * stopped, and the register at SIOCNT + 0xDA is set to 0xC0. */
    ST16(A_SIOControlRegister, 0x80 << 6);
    ST16(A_Timer3Control, 0);
    ST16(A_SIOControlRegister + 0xDA, 0xC0);

    /* BIOS-fill with zeros: 0x266 halfwords into the timer-enable block, and
     * 2 words into the SIO buffer; the fill sources are stack scratch slots
     * (only the low halfword of the second slot is written, as in the
     * original). */
    SP -= 8;
    ST32(SP, 0);
    call_SWI_CpuSet(SP, A_eTimerEnable200F460, 0x5000266);
    ST16(SP + 4, 0);
    last = call_SWI_CpuSet(SP + 4, A_unk_200F44C, 0x1000002);
    SP += 8;

    /* Clear the second byte of the SIO buffer header. */
    ST8(A_unk_200F44C + 1, 0);

    /* r1/r2/r3 exactly as the last BIOS CpuSet left them. */
    return last;
}

/* Entry: thin adapter. The original preserves r4/r5 (push/pop around the
 * body) and leaves lr alone, so the adapter does not touch them. Its last
 * flag-setting instruction is "mov r1, r4" (= adds r1, r4, #0) with
 * r4 = 0x200F44C, i.e. C = 0, V = 0 (and N = 0, Z = 0). */
Next sub_81440D8(void) {
    call_SWI_CpuSet_result out = reset_comm_hardware();
    R1 = out.r1;
    R2 = out.r2;
    R3 = out.r3;
    SET_FLAGS_ADD(A_unk_200F44C, 0);
    RETURN_TO_CALLER();
}

/* ── link_initAndClear_8144158 ── */
/* register form: calls SWI_CpuSet outside this file, which has no typed C interface yet.
 * CpuSet's fill source must be a guest address, so a zero is placed on the stack. */
static void cpu_set_fill_32(u32 dest, u32 mode) {
    SP -= 4;
    ST32(SP, 0);
    R0 = SP;
    R1 = dest;
    R2 = mode;
    GUEST_CALL(SWI_CpuSet);
    SP += 4;
}

/* register form: calls SWI_CpuSet outside this file, which has no typed C interface yet.
 * CpuSet's copy source must be a guest address, so a zero halfword is placed on the stack. */
static void cpu_set_copy_16(u32 dest, u32 mode) {
    SP -= 4;
    ST16(SP, 0);
    R0 = SP;
    R1 = dest;
    R2 = mode;
    GUEST_CALL(SWI_CpuSet);
    SP += 4;
}

/* Reset the GBA link-cable (SIO) hardware and clear all multiplayer communication
 * state.  Saves/restores IME around masked IE writes, puts SIO into general-purpose
 * mode, fires a dummy transfer to reset the state machine, re-enables the SIO
 * interrupt, zeroes the data registers and software communication buffers. */
static void init_sio_and_clear_link_state(void) {
    u32 ime = A_InterruptMasterEnableRegister;
    u32 ie  = A_InterruptEnableRegister;

    /* Save IME, mask out bits 6-7 from IE (disable SIO IRQ), restore IME */
    u16 ime_val = LD16(ime);
    ST16(ime, 0);
    ST16(ie, LD16(ie) & 0xFF3F);
    ST16(ime, ime_val);

    /* SIGMD = 0: general-purpose / multi-player data mode */
    ST16(A_SIOModeSelect_GeneralPurposeData, 0);

    /* SIOCTL: write SCER (0x2000), then start transfer by OR-ing SCST|SCCK|SCDT (0x4003) */
    u32 sioctl = A_SIOControlRegister;
    ST16(sioctl, 0x2000);
    ST16(sioctl, LD16(sioctl) | 0x4003);

    /* Stash IME in RAM, disable interrupts, re-enable SIO IRQ (bit 7 of IE), restore IME */
    ime_val = LD16(ime);
    ST16(A_word_2006DFC, ime_val);
    ST16(ime, 0);
    ST16(ie, LD16(ie) | 0x0080);
    ST16(ime, ime_val);

    /* Clear SIO data registers */
    ST16(A_SIOData_Normal_8bitandUARTMode_, 0);
    ST32(A_SIOData0_Parent__Multi_PlayerMode_, 0);
    ST32(A_SIOData0_Parent__Multi_PlayerMode_ + 4, 0);

    /* Zero-fill timer-enable table (614 32-bit words) and comm halfword buffer (2 halfwords) */
    cpu_set_fill_32(A_eTimerEnable200F460, 0x5000266);
    cpu_set_copy_16(A_unk_200F44C, 0x1000002);

    /* Clear per-link software state */
    ST8(A_byte_200F450, 0);
    ST8(A_byte_2006DFE, 0);
    ST8(A_byte_2006E00, 0);
    ST8(A_byte_200F440, 0);
    ST8(A_byte_200F448, 0);
    ST8(A_byte_2006E00 + 1, 0);   /* byte_2006E01 */
    ST16(A_word_2006E02, 0);
    ST16(A_word_2006E04, 0);
}

/* link_initAndClear_8144158() -> () ; no register inputs/outputs beyond preserved callee regs */
Next link_initAndClear_8144158(void) {
    init_sio_and_clear_link_state();
    RETURN_TO_CALLER();
}

/* ── sub_8144240 ── */
/* sub_8144240: run both steps of the (likely per-frame) update for this
 * subsystem: link_initAndClear_8144158 followed by sub_81440D8. Neither takes arguments;
 * whatever the callees leave in r2/r3 is passed through to the caller. */
Next sub_8144240(void) {
    GUEST_CALL(link_initAndClear_8144158);
    GUEST_CALL(sub_81440D8);
    RETURN_TO_CALLER();
}

/* ── ereader_updateTimerEnable_8144380 ── */
/* ereader_updateTimerEnable_8144380: update the e-Reader timer-enable flag (eTimerEnable200F460)
 * based on the current SIO mode. When the link port is in the 0x8 mode
 * and the secondary byte_200F462 flag is clear, the flag byte is set to
 * that mode value (0x8); in every other case it is cleared to 0. */
Next ereader_updateTimerEnable_8144380(void) {
    u32 sioMode = LD32(A_SIOControlRegister) & 0xC;
    u32 flagBase = A_eTimerEnable200F460;

    if (sioMode == 0x8 && LD8(flagBase + 2) == 0)
        ST8(flagBase, sioMode);
    else
        ST8(flagBase, 0);

    RETURN_TO_CALLER();
}

/* ── timer_IsEnabled_8144CE0 ── */
/* timer_IsEnabled_8144CE0: return the enabled state of the timer flags at
 * eTimerEnable200F460 (its second byte). */
Next timer_IsEnabled_8144CE0(void) {
    R0 = LD8(A_eTimerEnable200F460 + 1);
    RETURN_TO_CALLER();
}

/* ── rfu_handleVBlank_81453D4 ── */
/* rfu_handleVBlank_81453D4: Sync VBlank; if rfu_syncVBlank returned non-zero,
 * call sub_81461B0 with r0=0xF1, r1=0, then sub_81464B4. */
Next rfu_handleVBlank_81453D4(void) {
    GUEST_CALL(rfu_syncVBlank);
    if ((s32)R0 != 0) {
        R0 = 0xF1;
        R1 = 0;
        GUEST_CALL(sub_81461B0);
        GUEST_CALL(sub_81464B4);
    }
    RETURN_TO_CALLER();
}

/* ── sub_814692C ── */
/* generated wrappers of the game functions it calls (tools/abi/gen.py) */
typedef struct { u32 r4, r5, r6, r7, r8, r9, r10, r11, r12; } call_rfu_handleVBlank_81453D4_result;
/* register form: generated wrapper: rfu_handleVBlank_81453D4 has no typed C interface yet */
static inline call_rfu_handleVBlank_81453D4_result call_rfu_handleVBlank_81453D4(void) {
    GUEST_CALL(rfu_handleVBlank_81453D4);
    return (call_rfu_handleVBlank_81453D4_result){ R4, R5, R6, R7, R8, R9, R10, R11, R12 };
}

/* sub_814692C: tail-call trampoline into the RFU VBlank handler.
 *
 * The original is:  push {lr}; bl rfu_handleVBlank_81453D4; pop {r0}; bx r0
 * i.e. the return address is parked on the stack, the handler runs, and its
 * return lands on the pop/bx that jumps straight back to our caller. The
 * handler's r4-r12 are this function's results; r0-r3, lr and the flags are
 * clobbered, and the stack pointer ends where it started. */
static call_rfu_handleVBlank_81453D4_result run_rfu_vblank(u32 return_address) {
    /* register form: push {lr} lowers the guest SP and leaves the return
     * address on the guest stack across the call (the callee's own frame
     * sits below it), and the trailing pop {r0} removes it again, so the
     * guest SP must actually move here; no parameter can carry this. */
    SP -= 4;
    ST32(SP, return_address);

    call_rfu_handleVBlank_81453D4_result res = call_rfu_handleVBlank_81453D4();

    SP += 4;    /* pop {r0}: stack back to the caller's level */
    return res;
}

Next sub_814692C(void) {
    call_rfu_handleVBlank_81453D4_result res = run_rfu_vblank(LR);

    R4 = res.r4, R5 = res.r5, R6 = res.r6, R7 = res.r7;
    R8 = res.r8, R9 = res.r9, R10 = res.r10, R11 = res.r11, R12 = res.r12;

    RETURN_TO_CALLER();
}

/* ── sub_8146BF4 ── */
/* sub_8146BF4: read the state byte of the object referenced by dword_2010CC0
 * (dword_2010CC0 -> struct; first byte is its state code).
 * R0 = 1 if the state is 0 or 1, else 0. NZ/C/V are left as the original's
 * final CMP + MOV sequence leaves them so callers can branch on the result. */
Next sub_8146BF4(void) {
    u32 state = LD8(LD32(A_dword_2010CC0));

    SET_FLAGS_CMP(state, 1);
    if (state == 1) {
        R0 = 1;
    } else {
        SET_FLAGS_CMP(state, 0);
        if (state != 0)
            R0 = 0;
        else
            R0 = 1;
    }
    SET_FLAGS_NZ(R0);

    RETURN_TO_CALLER();
}

/* ── rfu_setTimerInterrupt ── */
/* rfu_setTimerInterrupt(r0, r1): set up the hardware timer used by the
 * wireless library, with the two arguments swapped and the first one
 * truncated to 8 bits before being handed to STWI_init_timer.
 * Whatever STWI_init_timer leaves in r2-r5 is passed through to our caller. */
Next rfu_setTimerInterrupt(void) {
    u32 argA = R0;
    u32 argB = R1;

    R0 = argB;
    R1 = argA & 0xFF;
    GUEST_CALL(STWI_init_timer);

    RETURN_TO_CALLER();
}

/* ── rfu_STC_REQ_callback ── */
/* rfu_STC_REQ_callback(r0 = requesting device id, r1 = request command):
 * RFU library callback invoked when a REQ packet arrives.  Reinstalls the
 * library's default async callback, records the request command in the RFU
 * work area, and — if the callback slot is armed (bit 3 of the work area's
 * status byte) — forwards the request to its handler.
 *
 * Flag outputs: on the dispatch path they are whatever sub_814DA68 leaves;
 * on the skip path they come from the arm-bit test (CMP against 0).
 * r4/r5 are preserved: the original pushes them around the call and pops
 * them back (sub_814DA68 sees reqCmd in r4 and srcId in r5).
 *
 * Note on the return: the original exits with `pop {r4,r5}` / `pop {r0}` /
 * `bx r0`.  Because its push saved lr as the third word, that `pop {r0}`
 * loads the *return address* into r0 — so r0 leaves the function holding
 * the entry LR value on both paths. */
Next rfu_STC_REQ_callback(void) {
    u32 srcId   = R0 & 0xFF;      /* byte argument   */
    u32 reqCmd  = R1 & 0xFFFF;    /* halfword argument */
    u32 entryLR = LR;             /* becomes the outgoing r0 (see note above) */
    u32 inR4 = R4, inR5 = R5;     /* entry values, restored on the dispatch path */

    /* Always (re)install the default serial callback. */
    R0 = A_rfu_CB_defaultCallback | 1u;
    GUEST_CALL(STWI_set_Callback_M);

    /* Record the request command in the RFU work area. */
    u32 rfuWork = LD32(A_dword_2010CC4);
    ST16(rfuWork + 0x1C, reqCmd);

    u32 status = LD8(rfuWork);
    if (status & 8) {
        /* Slot armed: dispatch (id, command, handler list pointer). */
        R0 = srcId;
        R1 = reqCmd;
        R2 = LD32(LD32(A_dword_2010CC8));
        R4 = reqCmd;
        R5 = srcId;
        GUEST_CALL(sub_814DA68);
        R4 = inR4;    /* original pops r4/r5 back over the callee's values */
        R5 = inR5;
    } else {
        /* Skip path: exit flags are those of the arm-bit test, r1 keeps
         * the whole status byte as loaded by `ldrb r1, [r0]`. */
        SET_FLAGS_CMP(status & 8, 0);
        R1 = status;
    }

    /* `pop {r0}; bx r0`: r0 exits holding the saved return address. */
    R0 = entryLR;
    RETURN_TO_CALLER();
}

/* ── STWI_init_timer ── */
/* STWI_init_timer(r0 = callback slot, r1 = timer id):
 * Install STWI_intr_timer as the handler stored at *r0 and record the timer id
 * in the STWI work area (dword_2010CCC). Then enable the timer's IRQ bit in
 * REG_IE, briefly disabling interrupts (REG_IME) around the update so the new
 * bit can't fire mid-write.
 *
 * Results left for the caller:
 *   r2 = REG_IME value as it was before the update
 *   r3 = &REG_IME (InterruptMasterEnableRegister)
 * r4/r5 are clobbered internally but restored, so they stay as they came in. */
Next STWI_init_timer(void) {
    u32 slot = R0;
    u32 timerId = R1;

    /* Hook the timer interrupt handler into the caller's callback slot. */
    ST32(slot, A_STWI_intr_timer | 1);

    /* Save the timer id in the STWI work area. */
    u32 workSlot = A_dword_2010CCC;
    u32 work = LD32(workSlot);
    ST8(work + 0xA, timerId);

    /* Turn interrupts off while we update the enable mask. */
    u32 imeReg = A_InterruptMasterEnableRegister;
    u32 savedIme = LD16(imeReg);
    ST16(imeReg, 0);

    /* Set the IRQ enable bit for the selected timer (bit 3 << timer id),
     * keeping any enables that were already requested. */
    u32 ieReg = A_InterruptEnableRegister;
    u32 idByte = LD8(work + 0xA);          /* reread the id just stored */
    u32 timerBit = LSL_REG(8, idByte & 0xFF);
    ST16(ieReg, timerBit | LD16(ieReg));

    /* Restore the previous interrupt master enable state. */
    ST16(imeReg, savedIme);

    R2 = savedIme;
    R3 = imeReg;
    RETURN_TO_CALLER();
}

/* ── STWI_init_Callback_M ── */
/* STWI_init_Callback_M: clear the wireless callback handler (pass NULL to
 * STWI_set_Callback_M) so no more STWI callback is invoked. */
Next STWI_init_Callback_M(void) {
    R0 = 0;
    GUEST_CALL(STWI_set_Callback_M);
    RETURN_TO_CALLER();
}

/* ── STWI_init_Callback_S ── */
/* STWI_init_Callback_S:
 * Registers the default (NULL) callback for the STWI wireless lib and
 * returns to our caller. Equivalent to STWI_set_Callback_S(NULL). */
Next STWI_init_Callback_S(void) {
    R0 = 0;
    GUEST_CALL(STWI_set_Callback_S);
    RETURN_TO_CALLER();
}

/* ── STWI_set_Callback_M ── */
/* STWI_set_Callback_M(r0 = callback function pointer):
 * Store the given callback into the STWI work struct's callback slot (offset 0x18),
 * so the wireless library invokes it when a message arrives. */
Next STWI_set_Callback_M(void) {
    u32 stwiWork = LD32(A_dword_2010CCC);

    ST32(stwiWork + 0x18, R0);

    RETURN_TO_CALLER();
}

/* ── STWI_set_Callback_S ── */
/* STWI_set_Callback_S(r0 = callback function pointer):
 * Register r0 as the "send done" callback in the STWI library's work struct. */
Next STWI_set_Callback_S(void) {
    u32 stwiWork = LD32(A_dword_2010CCC);
    ST32(stwiWork + 0x1C, R0); /* oStwiWork_Callback */
    RETURN_TO_CALLER();
}

/* ── STWI_set_Callback_ID ── */
/* STWI_set_Callback_ID(r0 = callback ID):
 * Store the given ID as the STWI (wireless) callback ID in the STWI work
 * struct pointed to by dword_2010CCC. */
Next STWI_set_Callback_ID(void) {
    ST32(LD32(A_dword_2010CCC) + 0x20, R0);
    RETURN_TO_CALLER();
}

/* ── Sio32IDInit ── */
/* Typed implementation of Sio32IDInit. The GBA serial I/O has to be put into a known state
 * before the link cable handshake, which is what this routine does. */
static void sio32_id_init(void) {
    /* Disable IRQs at the master enable while we reconfigure IE. */
    ST16(A_InterruptMasterEnableRegister, 0);

    /* Clear the multiplayer-SIO IRQ bit (bit 8 << multiplayer_id, with the id read from
     * the save data at dword_2010CCC + 0xA) and the generic SIO IRQ bit 0x80 in IE. */
    {
        u32 save_ptr = LD32(A_dword_2010CCC);
        u32 mask = (8u << LD8(save_ptr + 0xA)) | 0x80u;
        ST16(A_InterruptEnableRegister,
             LD16(A_InterruptEnableRegister) & (u16)~mask);
    }

    /* Re-enable IRQs at the master enable. */
    ST16(A_InterruptMasterEnableRegister, 1);

    /* Put the SIO mode select register into "general-purpose data" mode. */
    ST16(A_SIOModeSelect_GeneralPurposeData, 0);

    /* Program RCNT (InterruptEnableRegister - 0xD8) for multiplayer:
     * write 0x1000 (parent-to-child transfer enable), then OR in 0x4080 (multiplayer
     * mode flag), mirroring the original's read-modify-write sequence. */
    {
        u32 rcnt = (u32)A_InterruptEnableRegister - 0xD8;
        ST16(rcnt, 0x1000);
        ST16(rcnt, LD16(rcnt) | 0x4080);
    }

    /* Zero the 12-byte link ID buffer at byte_2010CD0. The original clears it with
     * SWI_CpuSet using a stack-zero source (count = 3 u32 words); the resulting
     * memory state is identical to writing the zeros directly. */
    ST32((u32)A_byte_2010CD0 + 0, 0);
    ST32((u32)A_byte_2010CD0 + 4, 0);
    ST32((u32)A_byte_2010CD0 + 8, 0);

    /* Acknowledge the SIO interrupt. */
    ST16(A_InterruptRequestFlags_IRQAcknowledge, 0x80);
}

Next Sio32IDInit(void) {
    sio32_id_init();
    RETURN_TO_CALLER();
}

/* ── Sio32IDMain ── */
/* Sio32IDMain (0x0814CD70): the 32-bit multiplayer ID handshake state machine.
 *
 * byte_2010CD0 is a small RAM control block; byte_2010CD1 holds the current
 * stage. r12 is set to that stage on entry and kept as a result (the original
 * never rewrites r12 on any path). r0 is the function's real return value.
 *
 *   stage 0 -> activate: set byte_2010CD0, raise the SIO start bit, bracket the
 *              IE SIO-enable with IME 0 then IME 1, advance the stage byte to 1,
 *              and finally set bit 7 of SIO's low byte. Returns 0.
 *   stage 1 -> if word_2010CDA (partner ID) is non-zero, mark the stage byte 2
 *              and return that partner ID. Otherwise if the active flag is 1,
 *              re-arm IME around SIO; else run the full teardown/restart: clear
 *              the SIO IRQ enable, drive RCNT 0 -> 0x1000 (32-bit parent clock),
 *              ack the SIO IRQ, raise RCNT with 0x4080, then re-enable the SIO
 *              IRQ inside a fresh IME bracket. Either way returns 0.
 *   any other stage -> just return the partner ID word.
 *
 * r4/r5/r6/r7 are saved and restored by the prologue/epilogue, so the caller's
 * values survive; only r0 and r12 are real results. The flags are clobbered and
 * callers do not read them. */

#define SIO32_ID_STATE_OFF   (A_byte_2010CD1  - A_byte_2010CD0)   /* stage byte   */
#define SIO32_ID_SEND_OFF    (A_word_2010CD2  - A_byte_2010CD0)   /* pending send */
#define SIO32_ID_MODE_OFF    (A_word_2010CD4  - A_byte_2010CD0)   /* session word */
#define SIO32_ID_PARTNER_OFF (A_word_2010CDA  - A_byte_2010CD0)   /* partner slot */

#define SIO32_ID_MODE_FULL     0x8001u    /* "already in the full session" sentinel */
#define SIO32_ID_START         0x0001u    /* RCNT start bit                */
#define SIO32_ID_ENABLE        0x0080u    /* IE SIO-IRQ enable / RCNT bit 7 */
#define SIO32_ID_CLR           0xFF7Fu    /* mask to clear IE bit 7        */
#define SIO32_ID_PARENT32      0x1000u    /* 32-bit multiplayer parent clock */
#define SIO32_ID_MULT32        0x4080u    /* multiplayer control packet    */

typedef struct {
    u32 r0;     /* partner ID (or 0 for the "busy/wait" paths) */
    u32 state;  /* stage byte as it was on entry (returned in r12) */
} Sio32Result;

/* The handshake, with base = the byte_2010CD0 control block. */
static Sio32Result sio32_id_run(u32 base) {
    u32 state = LD8(base + SIO32_ID_STATE_OFF);   /* captured before any write */
    u32 result;

    if (state == 0) {
        /* First entry: bring the SIO link up and advance the stage byte. */
        ST8(base, 1);
        ST16(A_SIOControlRegister,
             LD16(A_SIOControlRegister) | SIO32_ID_START);
        ST16(A_InterruptMasterEnableRegister, state);       /* mask IME (0) */
        ST16(A_InterruptEnableRegister,
             LD16(A_InterruptEnableRegister) | SIO32_ID_ENABLE);
        ST16(A_InterruptMasterEnableRegister, 1);           /* unmask IME (1) */
        ST8(base + SIO32_ID_STATE_OFF, 1);                  /* advance stage */
        ST8(A_SIOControlRegister,
            (u8)(LD8(A_SIOControlRegister) | SIO32_ID_ENABLE)); /* low-byte pulse */
        result = 0;
    } else if (state == 1) {
        u32 partner = LD16(base + SIO32_ID_PARTNER_OFF);
        if (partner != 0) {
            /* Partner slot filled: commit and surface it. */
            ST8(base + SIO32_ID_STATE_OFF, 2);
            result = partner;
        } else if (LD8(base) == 1) {
            /* Mode 1: pulse the SIO start bit under an IME bracket. */
            result = 0;
            if (LD16(base + SIO32_ID_SEND_OFF) == 0) {
                ST16(A_InterruptMasterEnableRegister, 0);
                ST16(A_SIOControlRegister,
                     LD16(A_SIOControlRegister) | SIO32_ID_ENABLE);
                ST16(A_InterruptMasterEnableRegister, state);
            }
        } else {
            /* Full session finalisation: tear SIO down, restart it in 32-bit
             * mode, ack the IRQ, then re-enable the SIO IRQ under IME. */
            result = 0;
            if (LD16(base + SIO32_ID_MODE_OFF) != SIO32_ID_MODE_FULL &&
                LD16(base + SIO32_ID_SEND_OFF) == 0) {
                ST16(A_InterruptMasterEnableRegister, 0);
                ST16(A_InterruptEnableRegister,
                     LD16(A_InterruptEnableRegister) & SIO32_ID_CLR);
                ST16(A_InterruptMasterEnableRegister, state);
                ST16(A_SIOControlRegister, 0);
                ST16(A_SIOControlRegister, SIO32_ID_PARENT32);
                ST16(A_InterruptRequestFlags_IRQAcknowledge, SIO32_ID_ENABLE);
                ST16(A_SIOControlRegister,
                     LD16(A_SIOControlRegister) | SIO32_ID_MULT32);
                ST16(A_InterruptMasterEnableRegister, 0);
                ST16(A_InterruptEnableRegister,
                     LD16(A_InterruptEnableRegister) | SIO32_ID_ENABLE);
                ST16(A_InterruptMasterEnableRegister, state);
            }
        }
    } else {
        /* Idle stage: just report the partner slot. */
        result = LD16(base + SIO32_ID_PARTNER_OFF);
    }

    Sio32Result out;
    out.r0    = result;
    out.state = state;
    return out;
}

/* Sio32IDMain() -> r0 = result, r12 = original stage byte. */
Next Sio32IDMain(void) {
    Sio32Result out = sio32_id_run(A_byte_2010CD0);
    R0  = out.r0;
    R12 = out.state;
    RETURN_TO_CALLER();
}

/* ── SWI_CpuFastSet ── */
/* SWI_CpuFastSet(r0 = src, r1 = dest, r2 = length | mode):
 * BIOS call 0xC. Copies or fills words in 8-word bursts.
 *   r2 bit 24 set: fill dest with the word at [src]; otherwise copy.
 * Length (bits 0-20) is rounded up by the BIOS to a multiple of 8 words;
 * both addresses must be 4-byte aligned. */
Next SWI_CpuFastSet(void) {
    swi_call(0xC);
    RETURN_TO_CALLER();
}

/* ── SWI_CpuSet ── */
/* SWI_CpuSet: thin wrapper around BIOS CpuSet (svc 0xB).
 *   r0 = source address (word-aligned for 32-bit transfers, halfword-aligned for 16-bit)
 *   r1 = destination address (same alignment rules)
 *   r2 = length/mode: bits 0-20 = element count, bit 24 = fixed source (fill),
 *        bit 26 = 32-bit transfers when set, 16-bit otherwise.
 * No return value; the result is the data written to the destination.
 * r0 is clobbered by the BIOS; r1-r3 pass through unchanged. */
Next SWI_CpuSet(void) {
    swi_call(0xB);
    RETURN_TO_CALLER();
}

/* ── SWI_Div ── */
/* SWI_Div(r0 = dividend, r1 = divisor):
 * Thin wrapper around the BIOS signed-division call (svc 6). The BIOS itself
 * produces the results: r0 = dividend / divisor, r1 = dividend % divisor
 * (both signed), and r3 = abs(quotient) as an unsigned value. r2 and lr are
 * passed through untouched. */
Next SWI_Div(void) {
    swi_call(6);
    RETURN_TO_CALLER();
}

/* ── libSave_CopyToGamePak ── */
/* libSave_CopyToGamePak(r0 = source, r1 = destination, r2 = byte count):
 * Copy `count` bytes (e.g. save data) to or from Game Pak address space.
 * While doing so, the WS0 field of the Game Pak waitstate control is forced
 * to 3 (the fastest setting) and left that way afterwards. The copy loop is
 * skipped entirely when count == 0. Always returns r3 = -1; r4/r5 (and the
 * waitstate bits above WS0) are preserved. */
Next libSave_CopyToGamePak(void) {
    u32 src = R0;
    u32 dst = R1;
    s32 count = (s32)R2;

    /* Force the Game Pak waitstates to 0b11 while touching the save chip. */
    u32 waitcnt = LD16(A_GamePakWaitstateControl);
    ST16(A_GamePakWaitstateControl, (waitcnt & 0xFFFCu) | 0x3u);

    /* Copy count bytes; the decrement-first structure means count == 0
     * copies nothing. */
    count--;
    if (count != -1) {
        do {
            ST8(dst, LD8(src));
            src++;
            dst++;
        } while (--count != -1);
    }

    R3 = 0xFFFFFFFF;
    RETURN_TO_CALLER();
}

/* ── umul3232H32 ── */
/* umul3232H32(r0, r1): 64-bit unsigned multiply r0 * r1.
 * Returns the high 32 bits of the product in r0; the low 32 bits are also
 * left in r2 and the high bits in r3 (as the original's umull does).
 * The original switches to ARM just to execute the single umull. */
Next umul3232H32(void) {
    u64 product = (u64)R0 * (u64)R1;
    R2 = (u32)product;        /* low word  */
    R3 = (u32)(product >> 32); /* high word */
    R0 = R3;                  /* result: high word */
    RETURN_TO_CALLER();
}

/* ── SoundMainBTM ── */
/* SoundMainBTM(r0 = sound work struct):
 * clear the 64-byte struct at r0 (four STM blocks of {r1-r4}), leaving r0
 * pointing just past it and r1-r3 zeroed. r4 is preserved; r12 receives the
 * incoming r4. Flags come from the final "movs r4, #0" (zero). */
Next SoundMainBTM(void) {
    u32 ptr = R0;

    R12 = R4;   /* saved r4, restored into r4 after the stores */
    R1 = 0;
    R2 = 0;
    R3 = 0;
    /* R4 is momentarily zeroed in the original, then restored from r12;
     * net effect: r4 unchanged. */

    for (u32 off = 0; off < 64; off += 4)
        ST32(ptr + off, 0);
    R0 = ptr + 64;

    SET_FLAGS_NZ(0);
    RETURN_TO_CALLER();
}

/* ── RealClearChain ── */
/* RealClearChain(r0 = chain node): unlink a node from its doubly linked list.
 *
 * Each node links to its list anchor at +0x2C, to the previous node at +0x30
 * and to the next node at +0x34; the anchor keeps the first node at +0x20.
 * If the node is attached, splice it out (fixing up the head pointer when it
 * was the first entry) and clear its anchor link. If it is not attached
 * (anchor == NULL), leave everything untouched.
 *
 * On return: r3 = the anchor link as read, r2 = the node's old "prev" and
 * r1 = its old "next" (both only written when the node was attached, r1 ends
 * as 0 afterwards). Flags: every path ends with Z set / C set / V clear,
 * i.e. "equal". */
Next RealClearChain(void) {
    R3 = LD32(R0 + 0x2C);               /* the list this node is attached to */

    if (R3 != 0) {
        R1 = LD32(R0 + 0x34);           /* next node in the chain */
        R2 = LD32(R0 + 0x30);           /* previous node in the chain */

        if (R2 != 0)
            ST32(R2 + 0x34, R1);        /* prev->next = next */
        else
            ST32(R3 + 0x20, R1);        /* node was first: update list head */

        if (R1 != 0)
            ST32(R1 + 0x30, R2);        /* next->prev = prev */

        R1 = 0;
        ST32(R0 + 0x2C, 0);             /* mark the node as detached */
    }

    /* Both exit paths leave the flags as if comparing 0 against 0. */
    SET_FLAGS_CMP(0, 0);
    RETURN_TO_CALLER();
}

/* ── sub_814E09C ── */
/* sub_814E09C: convert/copy the 36-word table at off_81C0418 into the buffer
 * whose address arrives in r0. Each word is fetched and then passed through
 * sub_814E0B6 (destination pointer in r0, source address in r2, word value in
 * r3; the converted word comes back in r3) before being stored. Like the
 * original, r12 is left holding the return address. */
Next sub_814E09C(void) {
    u32 dest = R0;
    u32 ret = LR;
    u32 src = A_off_81C0418;

    for (s32 count = 0x24; count > 0; count--) {
        u32 word = LD32(src);

        R0 = dest;
        R1 = (u32)count;
        R2 = src;
        R3 = word;
        R12 = ret;
        GUEST_CALL(sub_814E0B6);

        ST32(dest, R3);
        dest += 4;
        src += 4;
    }

    R12 = ret;
    RETURN_TO_CALLER();
}

/* ── sub_814E0B4 ── */
/* sub_814E0B4(r0 = arg, r2 = pointer): read the byte at *r2 into r3 and
 * tail-call sub_814E0B6, which continues the work with that byte.
 * r0 is passed through untouched; flags and results come from the callee. */
Next sub_814E0B4(void) {
    R3 = LD8(R2);
    GUEST_CALL(sub_814E0B6);
    RETURN_TO_CALLER();
}

/* ── sub_814E0B6 ── */
/* sub_814E0B6(r0 preserved, r2 = value to check, r3):
 * Range-check r2 and clear r3 when it is not an acceptable address.
 * Always returns with r0 unchanged. Flags reflect the last check performed:
 *   - if r2 has bit 25 set (>= 0x02000000) the check passes untouched and the
 *     flags come from that top-bit shift,
 *   - otherwise r2 is compared against the ROM constant off_81C0418; values
 *     below it fail (r3 = 0, flags from the comparison's MOV after it),
 *   - values at/above it are split on bit 14 (r2 >> 14) before r3 is cleared. */
Next sub_814E0B6(void) {
    u32 addr = R2;
    u32 saved_r0 = R0; /* the original pushes/pops r0 around its scratch use */

    /* Top-bit test: anything with bit 25 set is accepted as-is. */
    R0 = addr >> 25;
    SET_FLAGS_NZ(R0);
    cpu.c = (addr >> 24) & 1; /* LSR #25 carry: last bit shifted out */
    if (R0 != 0) {
        R0 = saved_r0;
        RETURN_TO_CALLER();
    }

    /* Below 0x02000000: compare against the ROM constant. */
    {
        u32 limit = A_off_81C0418;
        SET_FLAGS_CMP(addr, limit);
        if (addr < limit) {
            R3 = 0; /* mov r3,#0: N,Z only, C/V still from the CMP */
            SET_FLAGS_NZ(R3);
            R0 = saved_r0;
            RETURN_TO_CALLER();
        }
    }

    /* At/above the constant: shift test on bit 14, then clear r3 unless zero. */
    R0 = addr >> 14;
    SET_FLAGS_NZ(R0);
    cpu.c = (addr >> 13) & 1; /* LSR #14 carry: last bit shifted out */
    if (R0 != 0) {
        R3 = 0; /* mov r3,#0: N,Z only, C/V still from the shift/CMP */
        SET_FLAGS_NZ(R3);
    }
    R0 = saved_r0;
    RETURN_TO_CALLER();
}

/* ── ply_patt ── */
/* register form: calls ply_goto outside this file, which has no typed C interface yet */
static void call_ply_goto(u32 patt) {
    R1 = patt;
    GUEST_CALL(ply_goto);
}

/* register form: calls ply_fine outside this file, which has no typed C interface yet */
static void call_ply_fine(u32 patt) {
    R1 = patt;
    GUEST_CALL(ply_fine);
}

/* Advance one pattern step: store the next note address into the step's
 * slot and bump the step index, then hand off to ply_goto.
 * If the step index is already 3 (pattern exhausted), hand off to ply_fine. */
static void ply_patt_step(u32 patt) {
    u32 step = LD8(patt + 2);
    if (step >= 3) {
        call_ply_fine(patt);
        return;
    }
    u32 slot = patt + (step << 2);          /* slot at offset 4*step */
    u32 next_addr = LD32(patt + 0x40) + 4;  /* advance the read pointer by one note */
    ST32(slot + 0x44, next_addr);
    ST8(patt + 2, step + 1);
    call_ply_goto(patt);
}

/* ply_patt: 0814E0FC – advance pattern playback or signal completion. */
Next ply_patt(void) {
    ply_patt_step(R1);
    RETURN_TO_CALLER();
}

/* ── ply_pend ── */
/* ply_pend(r1 = pending-entry list):
 * Pop the most recently queued entry and make it the current one. The list
 * header holds a count at offset 2; the queued entries start at offset 0x44
 * (4 bytes each), and the "current" slot is at offset 0x40. An empty list is
 * left untouched (except that r2 still holds the count read from the header).
 * Flags come from the count test on the empty path and from the address
 * addition on the pop path. */
Next ply_pend(void) {
    u32 list = R1;

    R2 = LD8(list + 2);
    if (R2 == 0) {
        /* list empty: nothing to pop, only the count test's flags remain */
        SET_FLAGS_CMP(R2, 0);
        RETURN_TO_CALLER();
    }

    R2 = R2 - 1;
    ST8(list + 2, R2);

    R2 = R2 << 2;
    R3 = list + R2;
    SET_FLAGS_ADD(list, R2);

    R2 = LD32(R3 + 0x44);
    ST32(list + 0x40, R2);

    RETURN_TO_CALLER();
}

/* ── sub_814E168 ── */
/* sub_814E168: refresh two halfword fields of the object whose address
 * sub_814E0D0 returns in r0. Field +0x1C receives the callee's r3 doubled;
 * field +0x20 receives that doubled value scaled by the halfword at +0x1E
 * (an 8.8 fixed-point multiply: (doubled * scale) >> 8).
 * The original saves its return address into r12 and returns through it,
 * so r12 must end as the incoming lr; the callee preserves r12.
 * Flags come from the final shift: nz from the result, c from the last bit
 * shifted out (bit 7 of the product). The callee's r0/r2/r3 results stay in
 * r0/r2/r3 as the original leaves them. */
Next sub_814E168(void) {
    R12 = LR;   /* mov r12, lr — the original returns via bx r12 */

    GUEST_CALL(sub_814E0D0);

    u32 obj = R0;

    R3 <<= 1;                       /* doubled value from the callee */
    ST16(obj + 0x1C, R3);

    R2 = LD16(obj + 0x1E);          /* fixed-point scale factor */
    u32 product = R3 * R2;
    cpu.c = (product >> 7) & 1;     /* carry out of the lsr #8 */
    R3 = product >> 8;
    SET_FLAGS_NZ(R3);
    ST16(obj + 0x20, R3);

    RETURN_TO_CALLER();
}

/* ── sub_814E17C ── */
/* sub_814E17C(r1 = object, remaining inputs forwarded to sub_814E0D0):
 * Runs sub_814E0D0, which computes a byte value in r3 from the object state.
 * That byte is written to offset 0xA of the object, and the flag byte at
 * offset 0 gets its 0xC bits forced on (object marked as ...).
 * Returns r2 = 0xC, r3 = updated flag byte; NZ flags come from the ORR.
 * r1 and r12 pass through unchanged (sub_814E0D0 preserves both). */
Next sub_814E17C(void) {
    R12 = LR; /* sub_814E0D0 preserves r12; it still holds our return address */

    u32 obj = R1;
    GUEST_CALL(sub_814E0D0);

    ST8(obj + 0xA, R3);      /* store the computed byte */

    R2 = 0xC;
    R3 = LD8(obj) | R2;      /* set the 0xC bits in the object's flag byte */
    SET_FLAGS_NZ(R3);
    ST8(obj, R3);

    RETURN_TO_CALLER();
}

/* ── loadNextQueuedParams_814E190 ── */
/* loadNextQueuedParams_814E190(r0 = owner struct, r1 = work struct):
 * Read the next entry index from the byte queue at r1+0x40, then fetch the
 * three words of that 12-byte entry from the table pointed to by r0+0x30.
 * Each word is passed through sub_814E0B6 and stored to r1+0x24/0x28/0x2C.
 * Like the original, this uses r12 to hold the return address (mov r12, lr;
 * bx r12), so r12 must be set to the incoming LR before returning. */
Next loadNextQueuedParams_814E190(void) {
    R12 = LR;

    u32 cursor = LD32(R1 + 0x40);
    u32 index = LD8(cursor);          /* queue holds 8-bit entry numbers */
    ST32(R1 + 0x40, cursor + 1);

    u32 entry = LD32(R0 + 0x30) + index * 12;
    R2 = entry;

    R3 = LD32(entry + 0);
    GUEST_CALL(sub_814E0B6);
    ST32(R1 + 0x24, R3);

    R3 = LD32(entry + 4);
    GUEST_CALL(sub_814E0B6);
    ST32(R1 + 0x28, R3);

    R3 = LD32(entry + 8);
    GUEST_CALL(sub_814E0B6);
    ST32(R1 + 0x2C, R3);

    RETURN_TO_CALLER();
}

/* ── battleObj_runUpdateAndSetActive_814E1C0 ── */
/* battleObj_runUpdateAndSetActive_814E1C0: run sub_814E0D0's update for the object pointer in r1, then
 * store the byte result it leaves in r3 at offset 0x12 and set the low two
 * bits of the object's status byte (marking it active/updated).
 * Returns to the caller; flags come from the final ORR. */
Next battleObj_runUpdateAndSetActive_814E1C0(void) {
    R12 = LR;                    /* the original returns through a saved copy of LR */

    GUEST_CALL(sub_814E0D0);

    {
        u32 obj = R1;            /* object pointer, preserved by the callee */

        ST8(obj + 0x12, R3);     /* result byte produced by the call */

        R2 = 3;
        R3 = LD8(obj) | R2;      /* set status bits 0-1 */
        ST8(obj, R3);
        SET_FLAGS_NZ(R3);
    }

    RETURN_TO_CALLER();
}

/* ── sprite_moveYUp64AndMarkDirty_814E1D4 ── */
/* sprite_moveYUp64AndMarkDirty_814E1D4: adjust an object's Y coordinate and mark it as changed.
 * Calls sub_814E0D0 to compute a value (R1 = object pointer, R3 = value),
 * stores value - 0x40 (shifted up 64 pixels/units) into the object's
 * field at +0x14, then sets the low two bits of the object's byte at +0x00
 * (its "dirty"/state flags) so the change is picked up.
 * Returns via the saved LR in R12, as the original does with `bx r12`. */
Next sprite_moveYUp64AndMarkDirty_814E1D4(void) {
    R12 = LR;                     /* sub_814E0D0 clobbers LR; keep our return address */

    GUEST_CALL(sub_814E0D0);      /* R1 = object ptr, R3 = computed value */

    SET_FLAGS_CMP(R3, 0x40);      /* SUBS r3, #0x40 */
    R3 -= 0x40;
    ST8(R1 + 0x14, R3);

    R2 = 3;
    R3 = LD8(R1) | R2;            /* ORR r3, r2 sets N/Z only */
    SET_FLAGS_NZ(R3);
    ST8(R1, R3);

    RETURN_TO_CALLER();
}

/* ── sub_814E1E8 ── */
/* sub_814E1E8: run the check performed by sub_814E0D0, then record its
 * outcome in the state struct whose address is left in r1:
 *   - the status byte returned in r3, biased down by 0x40, is stored
 *     as the result field at offset 0x0E;
 *   - the flag bits 0x0C are set in the header byte at offset 0x00.
 * Returns to the caller through r12, like the original. */
Next sub_814E1E8(void) {
    R12 = LR; /* the original branches back through r12 */

    GUEST_CALL(sub_814E0D0);

    /* store the callee's status byte, biased down by 0x40 */
    u32 result = R3 - 0x40;
    SET_FLAGS_CMP(R3, 0x40); /* SUB r3, #0x40 sets c/v for the caller */
    ST8(R1 + 0xE, result);

    /* set the 0x0C flag bits in the struct's header byte */
    R2 = 0xC;
    u32 header = LD8(R1) | R2;
    SET_FLAGS_NZ(header); /* ORR is the original's last flag-setting op */
    ST8(R1, header);
    R3 = header;

    RETURN_TO_CALLER();
}

/* ── sub_814E1FC ── */
/* sub_814E1FC: finish some state update on the structure pointed to by r1.
 * Calls sub_814E0D0 (which produces a status byte in r3), stores that byte at
 * [r1+0xF], then sets the 0xC flag bits in the state byte at [r1].
 * Preserves the caller's return address in r12, as the original does. */
Next sub_814E1FC(void) {
    u32 state = R1;

    R12 = LR; /* mov r12, lr: keep the return address across the call */

    GUEST_CALL(sub_814E0D0); /* leaves the new status byte in R3 */

    ST8(state + 0xF, R3);

    R3 = LD8(state) | 0xC;
    R2 = 0xC;
    SET_FLAGS_NZ(R3);
    ST8(state, R3);

    RETURN_TO_CALLER();
}

/* ── sub_814E210 ── */
/* sub_814E210: wrapper around sub_814E0D0 — calls it, then stores the byte
 * result it leaves in r3 at [r1 + 0x1B]. Everything else (including r0 and
 * the flags left by the callee) passes through unchanged; returns to the
 * caller's original return address, kept in r12 like the original does. */
Next sub_814E210(void) {
    R12 = LR;
    GUEST_CALL(sub_814E0D0);
    ST8(R1 + 0x1B, R3);
    RETURN_TO_CALLER();
}

/* ── sub_814E21C ── */
/* sub_814E21C:
 * Calls sub_814E0D0 (which fills r1 with a record pointer and r3 with the
 * record's current/new state value), then syncs the state byte at [r1+0x18]:
 * if it differs from r3, the new value is written back and the low nibble of
 * the byte at [r1] is set to 0xF (marking the record as "needs refresh").
 * Flags are left as the last executed flag-setting instruction leaves them:
 * CMP r0, r3 on the equal path, ORR on the update path. */
Next sub_814E21C(void) {
    R12 = LR;
    GUEST_CALL(sub_814E0D0);

    R0 = LD8(R1 + 0x18);
    SET_FLAGS_CMP(R0, R3);
    if (R0 != R3) {
        ST8(R1 + 0x18, R3);
        R2 = 0xF;
        R3 = LD8(R1) | R2;
        ST8(R1, R3);
        SET_FLAGS_NZ(R3);
    }
    RETURN_TO_CALLER();
}

/* ── chatbox_PrintBuffer_WriteNextGlyph_814E234 ── */
/* chatbox_PrintBuffer_WriteNextGlyph_814E234: emit one character into the print buffer at r1.
 * Calls sub_814E0D0 to fetch the next glyph code (returned in r3),
 * stores the adjusted glyph index (code - 0x40) at [r1+0xC], then sets
 * bits 0x0C of the state byte at [r1] to mark that a glyph was written.
 * Preserves r1 and r12 across the call; returns to the original caller. */
Next chatbox_PrintBuffer_WriteNextGlyph_814E234(void) {
    R12 = LR; /* original return address; handed back unchanged on exit */

    GUEST_CALL(sub_814E0D0); /* -> R3 = glyph code */

    /* SUBS r3, #0x40: adjust the code and keep its carry/overflow flags,
     * since the ORR below only refreshes nz. */
    SET_FLAGS_CMP(R3, 0x40);
    R3 -= 0x40;
    ST8(R1 + 0xC, R3);

    /* set the "glyph written" flag bits in the state byte (r2 = 0xC,
     * as the original writes the constant via r2) */
    R2 = 0xC;
    R3 = LD8(R1) | R2;
    SET_FLAGS_NZ(R3);
    ST8(R1, R3);

    RETURN_TO_CALLER();
}

/* ── sprite_crossfadeColorChannels_814E528 ── */
/* Applies one channel of a color crossfade: multiply the source channel by a
 * fixed-point factor (12.14), then saturate at 0xFF. Only values above 0xFF
 * are clamped, matching the original. */
static u32 fade_channel(u32 channel, u32 factor) {
    u32 result = channel * factor;
    result = (u32)((s32)result >> 14);
    if (result > 0xFF)
        result = 0xFF;
    return result;
}

/* sprite_crossfadeColorChannels_814E528(r4 = destination color struct, r5 = source color struct):
 * Crossfade the two color bytes at [r4+2]/[r4+3] toward the source bytes at
 * [r5+0x10]/[r5+0x11]. The signed byte at [r4+0x14] is the fade position
 * (-0x80..0x7F): the first channel fades in with (0x80 + pos), the second
 * fades out with (0x7F - pos). The byte at [r4+0x12] scales both factors.
 * Leaves r2 = the sign-extended fade position. */
Next sprite_crossfadeColorChannels_814E528(void) {
    u32 weight = LD8(R4 + 0x12);
    u32 pos = (u32)(s8)LD8(R4 + 0x14);

    /* Channel 1 rises as the fade position moves from -0x80 toward +0x7F. */
    ST8(R4 + 2, fade_channel(LD8(R5 + 0x10), (0x80 + pos) * weight));

    /* Channel 2 falls over the same range. */
    ST8(R4 + 3, fade_channel(LD8(R5 + 0x11), (0x7F - pos) * weight));

    R2 = pos;
    RETURN_TO_CALLER();
}

/* ── sub_814E7A4 ── */
/* sub_814E7A4(r1 = status struct ptr):
 * Clear the two status bytes at +0x16 and +0x1A, then OR a mode flag into
 * byte 0: 0xC if byte +0x18 is zero, 3 otherwise.
 * Leaves r2 = mode flag, r3 = new byte 0.
 * Flags: N/Z from the OR result; the final ORR leaves C and V alone, so
 * they still hold what the "cmp r2, #0" left there (C set, V clear). */
Next sub_814E7A4(void) {
    ST8(R1 + 0x16, 0);
    ST8(R1 + 0x1A, 0);

    u32 delayByte = LD8(R1 + 0x18);
    SET_FLAGS_CMP(delayByte, 0);    /* C=1 (no borrow), V=0, kept by the ORR */

    u32 mode = (delayByte == 0) ? 0xC : 3;

    u32 flags = LD8(R1) | mode;
    ST8(R1, flags);

    R2 = mode;
    R3 = flags;
    SET_FLAGS_NZ(flags);

    RETURN_TO_CALLER();
}

/* ── streamReadNextByte_814E7C0 ── */
/* streamReadNextByte_814E7C0(r1 = stream state):
 * Reads the next byte from a byte stream whose current position is stored at
 * [r1+0x40], then advances the position. Outputs:
 *   r2 = address the byte was read from (position before the increment)
 *   r3 = the byte value itself. */
Next streamReadNextByte_814E7C0(void) {
    u32 stream = R1;
    u32 pos = LD32(stream + 0x40);
    ST32(stream + 0x40, pos + 1);
    R2 = pos;
    R3 = LD8(pos);
    RETURN_TO_CALLER();
}

/* ── sub_814E7CC ── */
/* sub_814E7CC: tail of a getter/setter pair.
 * Calls streamReadNextByte_814E7C0 (which returns a byte in r3 and a struct pointer in r1),
 * stores the byte at [r1 + 0x19], and if it was zero also calls
 * sub_814E7A4. r12 is kept as the original does (r12 = lr on entry). */
Next sub_814E7CC(void) {
    R12 = LR;

    GUEST_CALL(streamReadNextByte_814E7C0);

    ST8(R1 + 0x19, R3);
    if (R3 == 0) {
        /* Value was zero: run the extra fixup. Flags on this path are
         * whatever sub_814E7A4 leaves behind. */
        GUEST_CALL(sub_814E7A4);
    } else {
        /* Reproduce the `cmp r3, #0` for callers that branch on return. */
        SET_FLAGS_CMP(R3, 0);
    }

    RETURN_TO_CALLER(); /* original returns via `bx r12` with r12 == lr */
}

/* ── sub_814E7E0 ── */
/* sub_814E7E0: wrapper around streamReadNextByte_814E7C0.
 * Calls streamReadNextByte_814E7C0, stores its byte result at [r1 + 0x17], and — when the
 * result is zero — follows up with sub_814E7A4 before returning to the caller
 * (the return address is kept in r12 across both calls). */
Next sub_814E7E0(void) {
    R12 = LR;                       /* mov r12, lr: remember where to return */

    GUEST_CALL(streamReadNextByte_814E7C0);

    ST8(R1 + 0x17, R3);             /* store the byte result into the struct */
    SET_FLAGS_CMP(R3, 0);
    if (R3 == 0)
        GUEST_CALL(sub_814E7A4);    /* extra fixup only when result was zero */

    RETURN_TO_CALLER();             /* bx r12 */
}

/* ── battle_smashFx_SetParams_814E87C ── */
/* battle_smashFx_SetParams_814E87C(r0 = object, r1 = halfword value):
 * If the object's field at +0x34 holds the magic ID 'Smsh' (0x68736D53),
 * store the value (zero-extended to 16 bits) into fields +0x24 and +0x26
 * and store 0x100 into field +0x28.
 * The original pushes LR and pops it into r0 (`pop {r0}; bx r0`), so on
 * exit R0 is the return address. Flags reflect the magic comparison (or,
 * on the match path, the final `lsl r0, r0, #1`). */
Next battle_smashFx_SetParams_814E87C(void) {
    u32 return_address = LR; /* push {lr} ... pop {r0} */

    R2 = R0;
    R1 &= 0xFFFF; /* lsl #16 / lsr #16: keep only the low halfword */

    R3 = LD32(R2 + 0x34);
    R0 = 0x68736D53; /* 'Smsh' */
    SET_FLAGS_CMP(R3, R0);

    if (R3 == R0) {
        ST16(R2 + 0x26, R1);
        ST16(R2 + 0x24, R1);

        R0 = 0x80 << 1; /* 0x100; lsl sets C from the pre-shift top bit */
        cpu.c = 0;
        SET_FLAGS_NZ(R0);
        ST16(R2 + 0x28, R0);
    }

    R0 = return_address;
    RETURN_TO_CALLER();
}

/* ── sound_tickAllMusicPlayers_814EA58 ── */
/* sound_tickAllMusicPlayers_814EA58: walk the fixed music-player table gMPlayTable (0x20 entries,
 * 12 bytes each, first word is the player info pointer) and call
 * sub_814F1E8 on every entry. The entry count is a literal pool constant
 * in the original, so its "count == 0" early-out can never be taken.
 * On return r0 holds the return address: the original pops lr into r0
 * and returns with "bx r0", leaving r0 = lr. */

#define MPLAY_TABLE_COUNT      0x20u
#define MPLAY_TABLE_ENTRY_SIZE 0xCu

Next sound_tickAllMusicPlayers_814EA58(void) {
    const u32 *entry = (const u32 *)A_gMPlayTable;

    for (u32 i = MPLAY_TABLE_COUNT; i != 0; i--) {
        R0 = LD32((u32)entry);
        GUEST_CALL(sub_814F1E8);
        entry = (const u32 *)((u32)entry + MPLAY_TABLE_ENTRY_SIZE);
    }

    /* Flags on exit come from the loop's final "cmp r4, #0"; the loop
     * ends with the counter having just decremented to 0. */
    SET_FLAGS_CMP(0, 0);
    R0 = LR;
    RETURN_TO_CALLER();
}

/* ── cgbSoundInit_814EB84 ── */
/* register form: calls SWI_CpuSet (BIOS wrapper) outside this file;
 * needs SP because it passes a stack address as the 32-bit fill source.
 * Leaves R2/R3 as the callee's outputs (the caller reads those after the call). */
static void cpu_set_fill_zero(u32 dest, u32 mode) {
    SP -= 4;
    ST32(SP, 0);
    R0 = SP;
    R1 = dest;
    R2 = mode;
    GUEST_CALL(SWI_CpuSet);
    SP += 4;
}

/* Reset GBA sound hardware, verify the CGB-sound tag, patch the driver jump
 * table and state struct, zero-fill the 256-byte buffer, then stamp
 * channel-pattern bytes.  sound_buf is the caller-supplied buffer (r0). */
static void init_cgb_sound(u32 sound_buf) {
    /* Enable all channels (NR52 = 0x8F) */
    ST16(A_ControlSoundon_off_NR52_, 0x8F);

    /* Clear stereo/volume pair (NR50, NR51), then set NR50 master volume to 0x77 */
    u32 nr50 = A_ControlStereo_Volume_Enable_NR50_NR51_;
    ST16(nr50, 0);

    /* NR12/NR22/NR42 length = 8 */
    u32 addr = (A_Channel1Duty_Length_Envelope_NR11_NR12_ | 1);   /* NR12 + 1 */
    ST8(addr, 8);
    addr += 6;       /* NR22 + 1 */
    ST8(addr, 8);
    addr += 0x10;    /* NR42 + 1 */
    ST8(addr, 8);

    /* NR14/NR24/NR44 trigger = 0x80 */
    addr -= 0x14;    /* NR14 + 1 */
    ST8(addr, 0x80);
    addr += 8;       /* NR24 + 1 */
    ST8(addr, 0x80);
    addr += 0x10;    /* NR44 + 1 */
    ST8(addr, 0x80);

    /* NR34 (channel 3) off */
    addr -= 0xd;     /* NR34 + 1 */
    ST8(addr, 0);

    /* NR50 master volume */
    ST8(nr50, 0x77);

    /* Verify the "Shms" tag in the CGB sound state */
    u32 state_ptr = LD32(A_byte_3007FF0);
    u32 tag = LD32(state_ptr);
    if (tag != 0x68736D53)
        return;

    /* Temporarily bump tag to guard against re-entry while patching */
    ST32(state_ptr, tag + 1);

    /* Install patched sound-driver handlers in the game's function-pointer table */
    u32 fp = A_off_3005570;
    ST32(fp + 0x20, (A_sub_814FBFC | 1));
    ST32(fp + 0x44, (A_sub_814E7CC | 1));
    ST32(fp + 0x4C, (A_sub_814E7E0 | 1));
    ST32(fp + 0x70, (A_sub_814FD54 | 1));
    ST32(fp + 0x74, (A_sub_814E758 | 1));
    ST32(fp + 0x78, (A_SampleFreqSet_814EDC0 | 1));
    ST32(fp + 0x7C, (A_sub_814E4DC | 1));
    ST32(fp + 0x80, (A_sub_814F228 | 1));
    ST32(fp + 0x84, (A_sub_814F2F0 | 1));

    /* Wire state struct to patched handlers and the sound buffer */
    ST32(state_ptr + 0x1C, sound_buf);
    ST32(state_ptr + 0x28, (A_CgbSound | 1));
    ST32(state_ptr + 0x2C, (A_soundStartChannel_814F44C | 1));
    ST32(state_ptr + 0x30, (A_sub_814F3A4 | 1));
    ST8(state_ptr + 0x0C, 0);

    /* Zero-fill the 256-byte buffer (64 × 32-bit, fixed source/dest) */
    cpu_set_fill_zero(sound_buf, 0x05000040);

    /* Stamp channel-pattern identification bytes into the buffer */
    ST8(sound_buf + 0x01, 1);
    ST8(sound_buf + 0x1C, 0x11);
    ST8(sound_buf + 0x41, 2);
    ST8(sound_buf + 0x5C, 0x22);
    ST8(sound_buf + 0x81, 3);
    ST8(sound_buf + 0x9C, 0x44);
    ST8(sound_buf + 0xC1, 4);
    ST8(sound_buf + 0xDC, 0x88);

    /* Restore original tag (undo the temporary bump) */
    ST32(state_ptr, tag);
}

/* cgbSoundInit_814EB84(r0 = CGB sound buffer) -> r2, r3 (r4/r5/r6 preserved by push/pop).
 * Mismatch path: r2=0, r3=NR50 address (set by adapter before the call).
 * Match path:    r2/r3 left behind by SWI_CpuSet inside the typed function. */
Next cgbSoundInit_814EB84(void) {
    R2 = 0;
    R3 = A_ControlStereo_Volume_Enable_NR50_NR51_;
    init_cgb_sound(R0);
    RETURN_TO_CALLER();
}

/* ── m4aSoundVSync ── */
/* Offsets within the M4A driver state block (byte_3007FF0 holds a pointer to it). */
#define SOUNDINFO_MAGIC       0    /* u32: driver stamp, checked against the seed */
#define SOUNDINFO_FRAMES_LEFT 4    /* u8: VSyncs left before the DMA rekick */
#define SOUNDINFO_PERIOD      0xB  /* u8: value the frame counter reloads to */

/* The guard passes while (stamp + seed) wraps to 0 or 1, i.e. while the
 * driver has stamped the state block with its magic value. */
#define VSYNC_MAGIC_SEED 0x978C92ADu

#define DMA_WCOUNT_ENABLE 0x2000000u  /* 0x80 << 18: channel still armed */
#define DMA_WCOUNT_RELOAD 0x84400004u /* full-word reload written to an armed channel */
#define DMA_CTRL_DISABLE  0x0400u     /* 0x80 << 3: 32-bit transfer, channel off */
#define DMA_CTRL_RESTART  0xB600u     /* 0xB6 << 8: repeat + 32-bit + enable */

/* Everything the adapter must write back.  The original exits through
 * `pop {r0}; bx r0`, so r0 always comes out as the entry LR, and each exit
 * leaves the flags behind from a different instruction:
 *   magic-guard exit:  CMP  magic, #1   == ADDS magic, 0xFFFFFFFF
 *   countdown exit:    CMP  count, #0   == ADDS 0xFFFFFFFF, count + 1
 *   DMA rekick exit:   MOVS r0, 0xB600  == ADDS 0xB600, 0
 * flag_a/flag_b hold the ADDS operand pair for whichever exit was taken, so
 * the branch-free adapter reproduces N/Z/C/V with a single SET_FLAGS_ADD. */
typedef struct {
    u32 r0, r1, r2, r3;
    u32 flag_a, flag_b;
} SoundVSyncExit;

/* Per-frame service routine for the M4A sound driver: counts down a frame
 * gate and, once it expires, reloads it and restarts the two sound DMA
 * channels so Direct Sound streaming continues. */
static SoundVSyncExit m4a_sound_vsync_body(u32 entry_r3, u32 return_addr) {
    SoundVSyncExit out;

    u32 state = LD32(A_byte_3007FF0); /* pointer to the sound state block */

    /* Without the driver's stamp in the state block there is nothing to do. */
    u32 magic = LD32(state + SOUNDINFO_MAGIC) + VSYNC_MAGIC_SEED;
    if (magic > 1) {
        out.r0 = return_addr;
        out.r1 = VSYNC_MAGIC_SEED;     /* the seed add is the last thing r1 held */
        out.r2 = state;
        out.r3 = entry_r3;             /* untouched on this path */
        out.flag_a = magic;            /* CMP magic, #1 */
        out.flag_b = 0xFFFFFFFFu;
        return out;
    }

    /* Count this VSync down; while the counter is still positive the DMA
     * channels keep running and nothing else happens this frame. */
    u32 old_count = LD8(state + SOUNDINFO_FRAMES_LEFT);
    ST8(state + SOUNDINFO_FRAMES_LEFT, old_count - 1);
    u32 count_test = LD8(state + SOUNDINFO_FRAMES_LEFT) << 24; /* tested as signed */
    if ((s32)count_test > 0) {
        out.r0 = return_addr;
        out.r1 = old_count;            /* pre-decrement value, loaded for the strb */
        out.r2 = state;
        out.r3 = entry_r3;             /* untouched on this path */
        out.flag_a = 0xFFFFFFFFu;      /* CMP count, #0 */
        out.flag_b = count_test + 1;
        return out;
    }

    /* Gate expired: restore the frame period and rekick both DMA channels. */
    ST8(state + SOUNDINFO_FRAMES_LEFT, LD8(state + SOUNDINFO_PERIOD));

    u32 dma1_wcount = A_DMA1WordCount;
    if ((LD32(dma1_wcount) & DMA_WCOUNT_ENABLE) != 0)
        ST32(dma1_wcount, DMA_WCOUNT_RELOAD);
    u32 dma2_wcount = A_DMA2WordCount;
    if ((LD32(dma2_wcount) & DMA_WCOUNT_ENABLE) != 0)
        ST32(dma2_wcount, DMA_WCOUNT_RELOAD);

    u32 dma1_ctrl = A_DMA1Control;
    u32 dma2_ctrl = A_DMA2Control;
    ST16(dma1_ctrl, DMA_CTRL_DISABLE);
    ST16(dma2_ctrl, DMA_CTRL_DISABLE);
    ST16(dma1_ctrl, DMA_CTRL_RESTART);
    ST16(dma2_ctrl, DMA_CTRL_RESTART);

    out.r0 = return_addr;
    out.r1 = dma2_ctrl;                /* the DMA addresses/word are what the
                                          rekick sequence last left in r1..r3 */
    out.r2 = dma1_ctrl;
    out.r3 = DMA_CTRL_RESTART;
    out.flag_a = DMA_CTRL_RESTART;     /* MOVS r0, r3 */
    out.flag_b = 0;
    return out;
}

/* m4aSoundVSync() -- VSync hook of the M4A sound driver.
 * Inputs: lr (pushed around the body; the final pop loads it into r0).
 * Outputs: nz c v r0 r1 r2 r3. */
Next m4aSoundVSync(void) {
    SoundVSyncExit out = m4a_sound_vsync_body(R3, LR);
    R0 = out.r0;
    R1 = out.r1;
    R2 = out.r2;
    R3 = out.r3;
    SET_FLAGS_ADD(out.flag_a, out.flag_b);
    RETURN_TO_CALLER();
}

/* ── sub_814F3A4 ── */
/* sub_814F3A4 – packed-table interpolation lookup.
 *
 * mode (r0 byte) selects the table.
 *  Mode 4: clamp index to byte_81C0640 (simple 1-byte LUT).
 *  Other:  clamp index to 0x82, then interpolate between two adjacent
 *          entries of the packed table byte_81C05A4, whose low nibble
 *          indexes an s16 pool (byte_81C0628) and high nibble is an
 *          ASR amount; scale by the fraction byte (r2).
 */

#define MODE_SIMPLE   4

#define TABLE_SIMPLE  A_byte_81C0640   /* direct byte lookup   */
#define TABLE_PACKED  A_byte_81C05A4   /* packed entry table   */
#define TABLE_S16     A_byte_81C0628   /* s16 value pool       */

struct LookupResult {
    u32 r0, r2, r3, r12;
    u32 add_a, add_b;   /* operands of the final ADDS that sets nz/c/v */
};

static struct LookupResult do_lookup(u32 mode, u32 index, u32 frac, u32 r3_in)
{
    struct LookupResult res;

    if (mode == MODE_SIMPLE) {
        /* ── simple byte-table path ── */
        u32 idx = index;
        if (idx <= 0x14)
            idx = 0;
        else {
            idx = (idx - 0x15) & 0xFF;
            if (idx > 0x3B) idx = 0x3B;
        }

        res.r0  = LD8(TABLE_SIMPLE + idx);
        res.r2  = frac;
        res.r3  = r3_in;            /* unchanged */
        res.r12 = frac;
        res.add_a = idx;
        res.add_b = TABLE_SIMPLE;   /* flags from ADDS(idx, table_addr) */
        return res;
    }

    /* ── interpolation path ── */
    u32 idx   = index;
    u32 scale = frac;

    if (idx <= 0x23) {
        idx   = 0;
        scale = 0;
    } else {
        idx = (idx - 0x24) & 0xFF;
        if (idx > 0x82) {
            idx   = 0x82;
            scale = 0xFF;
        }
    }

    res.r3  = TABLE_PACKED;
    res.r12 = scale;
    res.r2  = 0;                    /* left at 0 after final "mov r2,#0" */

    /* Decode packed entry at idx → s32 value_a */
    u32 pk_a = LD8(TABLE_PACKED + idx);
    s32 va   = LDS16(TABLE_S16 + ((pk_a & 0xF) << 1));
    va = (s32)va >> (s32)(pk_a >> 4);

    /* Decode packed entry at idx+1 → s32 value_b */
    u32 pk_b = LD8(TABLE_PACKED + idx + 1);
    s32 vb   = LDS16(TABLE_S16 + ((pk_b & 0xF) << 1));
    vb = (s32)vb >> (s32)(pk_b >> 4);

    /* Linear interpolation: va + ((scale * (vb - va)) >> 8) + 0x800 */
    s32 delta   = vb - va;
    s32 product = (s32)scale * delta;
    s32 interp  = va + (product >> 8);

    res.r0    = (u32)(interp + 0x800);
    res.add_a = (u32)interp;
    res.add_b = 0x800;              /* flags from ADDS(interp, 0x800) */
    return res;
}

/* sub_814F3A4(r0=mode, r1=index, r2=frac, r3=passthru, lr) */
Next sub_814F3A4(void) {
    struct LookupResult lr_ = do_lookup(
        (R0 << 24) >> 24, (R1 << 24) >> 24, (R2 << 24) >> 24, R3);
    R0 = lr_.r0;  R1 = LR;  R2 = lr_.r2;  R3 = lr_.r3;  R12 = lr_.r12;
    SET_FLAGS_ADD(lr_.add_a, lr_.add_b);
    RETURN_TO_CALLER();
}

/* ── soundStartChannel_814F44C ── */
/* Structured result of the APU channel-start helper. */
struct SoundChanResult {
    u32 r1;       /* address of the last byte written (the trigger/consecutive register) */
    u32 add_a;    /* SET_FLAGS_ADD operands that reproduce the original exit flags        */
    u32 add_b;
};

/* Write the envelope/length and trigger bytes for one APU sound channel.
 * channel is masked to its low byte; the mapping is:
 *   1 → NR12 (len/env) then NR14 (trigger = 0x80)
 *   2 → NR22 (len/env) then NR24 (trigger = 0x80)
 *   3 → NR30 = 0 (stop wave channel; no separate trigger write)
 *   0 or >3 → NR42 (len/env) then NR44 (trigger = 0x80)
 * Returns the final register address and the ADDS operands whose flags match the
 * original's last flag-setting instruction on each path. */
static struct SoundChanResult configure_channel(u32 channel) {
    channel &= 0xFF;
    struct SoundChanResult res;
    u32 reg;

    if (channel == 1) {
        reg = A_Channel1Duty_Length_Envelope_NR11_NR12_ + 1;
        ST8(reg, 8);           /* envelope / length on NR12 */
        reg += 2;              /* ADDS r1,#2 → flags */
        ST8(reg, 0x80);        /* trigger on NR14 */
        res.add_a = reg - 2;
        res.add_b = 2;
    } else if (channel == 2) {
        reg = A_Channel2Duty_Length_Envelope_NR21_NR22_ + 1;
        ST8(reg, 8);           /* envelope / length on NR22 */
        reg += 4;              /* ADDS r1,#4 → flags */
        ST8(reg, 0x80);        /* trigger on NR24 */
        res.add_a = reg - 4;
        res.add_b = 4;
    } else if (channel == 3) {
        reg = A_Channel3Stop_WaveRAMselect_NR30_;
        ST8(reg, 0);           /* stop wave channel */
        /* Exit flags were set by CMP r1,#3 (with r1==3), which is equivalent to
         * ADDS 0xFFFFFFFF,1 → N=0 Z=1 C=1 V=0. */
        res.add_a = 0xFFFFFFFF;
        res.add_b = 1;
    } else {
        /* channel 0 or > 3 → use channel 4 */
        reg = A_Channel4Length_Envelope_NR41_NR42_ + 1;
        ST8(reg, 8);           /* envelope / length on NR42 */
        reg += 4;              /* ADDS r1,#4 → flags */
        ST8(reg, 0x80);        /* trigger on NR44 */
        res.add_a = reg - 4;
        res.add_b = 4;
    }

    res.r1 = reg;
    return res;
}

/* soundStartChannel_814F44C(r0 = channel number, lr = return address) → r0=lr, r1=trigger addr, flags */
Next soundStartChannel_814F44C(void) {
    struct SoundChanResult res = configure_channel(R0);
    R0 = LR;                    /* pop {r0} restores the caller's return address */
    R1 = res.r1;
    SET_FLAGS_ADD(res.add_a, res.add_b);
    RETURN_TO_CALLER();
}

/* ── soundChannel_tickPanVolume_814F4A0 ── */
/* soundChannel_tickPanVolume_814F4A0: per-tick update of one sound channel's pan / volume / mute bytes.
 *
 * inputs:  r0 = channel struct pointer
 * outputs: r2 r3 r4 (preserved; the typed function never touches them)
 * clobbers: r0 r1
 *
 * Reads pan bytes at [+2] and [+3], picks a "mute" byte (0xF0 / 0x0F / 0xFF),
 * stores it at +0x1B, computes avg = (a+b)>>4 and stores at +0xA (clamped to
 * 0xF only on the 0xF0/0x0F path), then writes (vol*avg + 0xF)>>4 at +0x19 and
 * updates +0x1B with (LD8(+0x1B) & LD8(+0x1C)) — the channel mute mask. */
static void sub_814F4A0_impl(u32 channel) {
    u8 a = LD8(channel + 2);
    u8 b = LD8(channel + 3);

    u8 mute;
    u32 avg;

    if (b > a) {
        /* av < bv branch (loc_814F4C0): compare (b>>1) against a */
        if ((b >> 1) >= a) {
            mute = 0xF0;
            ST8(channel + 0x1B, mute);
            /* loc_814F4DA: recompute, store, possibly clamp */
            avg = ((u32)LD8(channel + 2) + (u32)LD8(channel + 3)) >> 4;
            ST8(channel + 0xA, (u8)avg);
            if (avg > 0xF) {
                avg = 0xF;
                ST8(channel + 0xA, (u8)avg);
            }
        } else {
            /* loc_814F4CC (0xFF): store mute, store unclamped avg, no clamp */
            mute = 0xFF;
            ST8(channel + 0x1B, mute);
            avg = ((u32)a + (u32)b) >> 4;
            ST8(channel + 0xA, (u8)avg);
        }
    } else {
        /* av >= bv branch: compare (a>>1) against b */
        if ((a >> 1) >= b) {
            mute = 0x0F;
            ST8(channel + 0x1B, mute);
            /* loc_814F4DA: recompute, store, possibly clamp */
            avg = ((u32)LD8(channel + 2) + (u32)LD8(channel + 3)) >> 4;
            ST8(channel + 0xA, (u8)avg);
            if (avg > 0xF) {
                avg = 0xF;
                ST8(channel + 0xA, (u8)avg);
            }
        } else {
            /* loc_814F4CC (0xFF) */
            mute = 0xFF;
            ST8(channel + 0x1B, mute);
            avg = ((u32)a + (u32)b) >> 4;
            ST8(channel + 0xA, (u8)avg);
        }
    }

    /* Scale volume by the stored average: store ((vol*avg)+0xF)>>4 at +0x19,
     * then AND-mask the mute byte with the channel mute flag at +0x1C. */
    u32 ab = LD8(channel + 0xA);
    u32 vol = LD8(channel + 6);
    u32 scaled = ((vol * ab) + 0xF) >> 4;
    ST8(channel + 0x19, (u8)scaled);

    u32 cur_mute = LD8(channel + 0x1B);
    u32 mask = LD8(channel + 0x1C);
    ST8(channel + 0x1B, (u8)(mask & cur_mute));
}

/* Thin adapter. r2/r3/r4 are listed as outputs (preserved); the typed function
 * never touches them, so its presence automatically keeps them unchanged.
 * The original returns no flags or scalar value — just falls off the tail. */
Next soundChannel_tickPanVolume_814F4A0(void) {
    sub_814F4A0_impl(R0);
    RETURN_TO_CALLER();
}

/* ── text_ReadNextChar_814FE34 ── */
/* text_ReadNextChar_814FE34(r1 = text state):
 * Read the next character of a text script: load the byte at the current
 * read pointer, copy it to the state's current-character field, then advance
 * the pointer past it.
 * Returns r0 = advanced read pointer, r2 = the byte that was read;
 * flags reflect the pointer increment (ADDS). */
Next text_ReadNextChar_814FE34(void) {
    u32 state = R1;

    u32 readPos = LD32(state + 0x40);
    u32 ch = LD8(readPos);
    ST8(state + 0x1E, ch);

    readPos = readPos + 1;
    SET_FLAGS_ADD(readPos, 1);
    ST32(state + 0x40, readPos);

    R0 = readPos;
    R2 = ch;
    RETURN_TO_CALLER();
}

/* ── readNextStreamByte_814FE40 ── */
/* readNextStreamByte_814FE40(r1 = record pointer):
 * Copies the next byte of the data stream referenced by [r1+0x40] into the
 * byte field at [r1+0x1F], then advances the stream pointer past it.
 * Outputs: r0 = advanced stream pointer, r2 = the byte that was read;
 * nz/c/v come from the pointer increment (ADDS r0, #1). */
Next readNextStreamByte_814FE40(void) {
    u32 streamPos = LD32(R1 + 0x40);
    u8 value = LD8(streamPos);

    ST8(R1 + 0x1F, value);

    streamPos += 1;
    ST32(R1 + 0x40, streamPos);

    R0 = streamPos;
    R2 = value;
    SET_FLAGS_ADD(streamPos, 1);
    RETURN_TO_CALLER();
}
