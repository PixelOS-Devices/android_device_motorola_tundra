/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Replacement for the libdlbpreg blob. Its PregainSynchronizer handler thread
 * ("Dolby Pregain H") spins a CPU core at 100% for as long as audio plays
 * with Dolby on: the phone overheats, thermal mitigation caps the CPUs and
 * cameras in apps stutter. Same interface, no thread: libswdap, libdlbvol
 * and libswspatializer still load, pregain updates are dropped.
 */

#include <pthread.h>
#include <stdint.h>

#define EXPORT __attribute__((visibility("default")))

/* android::Singleton<PregainSynchronizer>, read inline by the callers */
EXPORT pthread_mutex_t singleton_lock __asm__("_ZN7android9SingletonINS_19PregainSynchronizerEE5sLockE") =
        PTHREAD_MUTEX_INITIALIZER;
EXPORT void* singleton_instance __asm__("_ZN7android9SingletonINS_19PregainSynchronizerEE9sInstanceE");

/* PregainSynchronizer::PregainSynchronizer() */
EXPORT void ctor(void* self) __asm__("_ZN7android19PregainSynchronizerC1Ev");
void ctor(void* self) { (void)self; }

EXPORT void ctor2(void* self) __asm__("_ZN7android19PregainSynchronizerC2Ev");
void ctor2(void* self) { (void)self; }

/* registerGlobalEffectImpl(DAP_EFFECT_TYPE, std::shared_ptr<...>&) */
EXPORT void reg(void* self, int type, void* effect) __asm__("_ZN7android19PregainSynchronizer24registerGlobalEffectImplENS_15DAP_EFFECT_TYPEERNSt3__110shared_ptrINS_12EffectItfCtxEEE");
void reg(void* self, int type, void* effect) {
    (void)self; (void)type; (void)effect;
}

/* unregisterGlobalEffectImpl(DAP_EFFECT_TYPE) */
EXPORT void unreg(void* self, int type) __asm__("_ZN7android19PregainSynchronizer26unregisterGlobalEffectImplENS_15DAP_EFFECT_TYPEE");
void unreg(void* self, int type) { (void)self; (void)type; }

/* sendPregainImpl(int, bool, unsigned int, int) */
EXPORT void send(void* self, int id, _Bool on, unsigned int gain, int ch) __asm__("_ZN7android19PregainSynchronizer15sendPregainImplEibji");
void send(void* self, int id, _Bool on, unsigned int gain, int ch) {
    (void)self; (void)id; (void)on; (void)gain; (void)ch;
}
