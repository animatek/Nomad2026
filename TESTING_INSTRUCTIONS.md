# Instrucciones para Probar Sincronización

## ⚠️ IMPORTANTE: Cómo Ver los Logs

Los logs **NO aparecen** si ejecutas la app haciendo doble clic. Debes ejecutar desde la **terminal**.

### Opción 1: Ejecutar desde Terminal (RECOMENDADO)

```bash
cd /mnt/SPEED/CODE/Nomad2026
./build/Nomad2026_artefacts/Release/Nomad2026
```

Verás logs como:
```
[SYNC] Patch synchronizer enabled after patch load from synth
[SYNC] Sent CableInsert: section=1 color=2 modules=3->5 types=out->in connectors=0->2
[SYNC]   SysEx: f0 33 06 17 50 7a 03 00 05 02 4b f7
[SYNC] Sent ModuleMove: section=0 module=2 pos=(3,1)
[SYNC]   SysEx: f0 33 06 17 34 00 02 03 01 43 f7
[SYNC] Sent StorePatch: slot=0 section=0 position=0
[SYNC]   StorePatch SysEx: f0 33 06 17 41 0b 00 00 00 XX f7
```

### Opción 2: Script Automático

```bash
cd /mnt/SPEED/CODE/Nomad2026
./run_debug.sh
```

---

## 🧪 Test 1: Verificar que el Synchronizer se Crea

1. **Ejecutar desde terminal**
2. **Conectar al sintetizador** (Device → MIDI Settings)
3. **Cargar un patch** (Device → Request Patch from Synth)
4. **BUSCAR EN LA TERMINAL:**
   ```
   [SYNC] Patch synchronizer enabled after patch load from synth
   ```

**✅ Si ves este mensaje:** El synchronizer se creó correctamente

**❌ Si NO lo ves:** Hay un problema al crear el synchronizer

---

## 🧪 Test 2: Verificar Envío de CableInsert

1. **Con el patch cargado, añadir un cable**
   - Arrastra desde cualquier salida a cualquier entrada

2. **BUSCAR EN LA TERMINAL:**
   ```
   [SYNC] Sent CableInsert: section=1 color=X modules=Y->Z types=out->in connectors=A->B
   [SYNC]   SysEx: f0 33 06 17 50 ...
   ```

**✅ Si ves estos mensajes:**
- El cable se está enviando al sintetizador
- Copia el mensaje SysEx completo (todos los bytes en hexadecimal)
- El formato puede estar bien o mal (lo verificaremos)

**❌ Si NO ves nada:**
- Los callbacks NO se están disparando
- El synchronizer no está recibiendo eventos
- Hay un problema en el event system

---

## 🧪 Test 3: Verificar Envío de ModuleMove

1. **Arrastra un módulo a una nueva posición**
2. **Suelta el mouse**
3. **BUSCAR EN LA TERMINAL:**
   ```
   [SYNC] Sent ModuleMove: section=X module=Y pos=(X,Y)
   [SYNC]   SysEx: f0 33 06 17 34 ...
   ```

**✅ Si ves el mensaje:** El evento de movimiento funciona

**❌ Si NO ves nada:** El callback de movimiento no se está disparando

---

## 🧪 Test 4: Verificar StorePatch

1. **Device → Send Patch to Synth**
2. **BUSCAR EN LA TERMINAL:**
   ```
   [SYNC] Sent StorePatch: slot=0 section=0 position=0
   [SYNC]   StorePatch SysEx: f0 33 06 17 41 0b 00 00 00 XX f7
   ```

**✅ Si ves el mensaje:** StorePatch se envía

**❌ Si NO ves nada:** Hay un problema con sendRawSysEx

---

## 🔍 Qué Compartir si No Funciona

### Si ves los mensajes [SYNC] pero los cambios no se guardan:

**Copia y pega EXACTAMENTE estos mensajes:**
```
[SYNC]   SysEx: f0 33 06 17 50 ...    <- CableInsert completo
[SYNC]   SysEx: f0 33 06 17 34 ...    <- ModuleMove completo
[SYNC]   StorePatch SysEx: f0 33 ...  <- StorePatch completo
```

Esto nos permitirá verificar byte por byte si el formato es correcto.

### Si NO ves los mensajes [SYNC]:

**Comparte:**
1. ¿Aparece "[SYNC] Patch synchronizer enabled"?
2. ¿Qué acción hiciste? (añadir cable, mover módulo, etc.)
3. ¿Algún error en la terminal?

---

## 📋 Checklist Completo

Ejecuta esta secuencia y anota qué ves:

- [ ] Ejecutar desde terminal
- [ ] Conectar a sintetizador
- [ ] Ver mensaje: `[SYNC] Patch synchronizer enabled` ← ¿APARECE?
- [ ] Request Patch from Synth
- [ ] Ver mensaje: `[SYNC] Patch synchronizer enabled after patch load` ← ¿APARECE?
- [ ] Añadir cable
- [ ] Ver mensaje: `[SYNC] Sent CableInsert:` ← ¿APARECE?
- [ ] Ver mensaje: `[SYNC]   SysEx: f0 33 06...` ← ¿APARECE?
- [ ] Mover módulo
- [ ] Ver mensaje: `[SYNC] Sent ModuleMove:` ← ¿APARECE?
- [ ] Send Patch to Synth
- [ ] Ver mensaje: `[SYNC] Sent StorePatch:` ← ¿APARECE?

---

## 🎯 Escenarios Posibles

### Escenario A: No aparece ningún mensaje [SYNC]
**Problema:** El synchronizer no se está creando
**Causa:** Problema en MainComponent o ConnectionManager

### Escenario B: Aparece "enabled" pero no aparecen "Sent"
**Problema:** Los callbacks no se disparan
**Causa:** Problem en el event system (Patch.cpp)

### Escenario C: Aparecen los "Sent" pero cambios no persisten
**Problema:** Formato de mensaje incorrecto O sintetizador los rechaza
**Causa:** Necesitamos verificar el SysEx byte por byte

### Escenario D: Todo aparece correcto pero igual no funciona
**Problema:** Posible problema en sendRawSysEx o MidiDeviceManager
**Causa:** Los mensajes no llegan realmente al MIDI output

---

## 📞 Siguiente Paso

**Ejecuta la aplicación desde terminal y comparte:**

1. ¿Qué mensajes [SYNC] aparecen?
2. Si aparecen SysEx bytes, copia el mensaje COMPLETO
3. ¿En qué escenario (A, B, C, o D) estás?

¡Con esta información podremos diagnosticar exactamente qué está pasando!
